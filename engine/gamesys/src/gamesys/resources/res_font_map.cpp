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
    struct Resources
    {
        MaterialResource*  m_MaterialResource;
        GlyphBankResource* m_GlyphBankResource;
    };

    static void ReleaseResources(dmResource::HFactory factory, void* font_map_user_data)
    {
        Resources* resources = (Resources*) font_map_user_data;
        dmResource::Release(factory, (void*) resources->m_MaterialResource);
        dmResource::Release(factory, (void*) resources->m_GlyphBankResource);
        delete resources;
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRender::HRenderContext context,
        dmRenderDDF::FontMap* ddf, dmRender::HFontMap font_map, const char* filename, dmRender::HFontMap* font_map_out, bool reload)
    {
        *font_map_out = 0;

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
            dmDDF::FreeMessage(ddf);
            return result;
        }

        dmRenderDDF::GlyphBank* glyph_bank = glyph_bank_res->m_DDF;
        assert(glyph_bank);

        dmRender::FontMapParams params;
        params.m_Glyphs.SetCapacity(glyph_bank->m_Glyphs.m_Count);
        params.m_Glyphs.SetSize(glyph_bank->m_Glyphs.m_Count);
        for (uint32_t i = 0; i < glyph_bank->m_Glyphs.m_Count; ++i)
        {
            dmRenderDDF::GlyphBank::Glyph& i_g = glyph_bank->m_Glyphs[i];
            dmRender::Glyph& o_g = params.m_Glyphs[i];
            o_g.m_Character = i_g.m_Character;
            o_g.m_Advance = i_g.m_Advance;
            o_g.m_Ascent = i_g.m_Ascent;
            o_g.m_Descent = i_g.m_Descent;
            o_g.m_LeftBearing = i_g.m_LeftBearing;
            o_g.m_Width = i_g.m_Width;

            o_g.m_InCache = false;
            o_g.m_GlyphDataOffset = i_g.m_GlyphDataOffset;
            o_g.m_GlyphDataSize = i_g.m_GlyphDataSize;

        }

        // Font map
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
        params.m_GlyphData          = glyph_bank->m_GlyphData.m_Data;

        if (font_map == 0)
        {
            font_map = dmRender::NewFontMap(dmRender::GetGraphicsContext(context), params);
        }
        else
        {
            dmRender::SetFontMap(font_map, params);
            ReleaseResources(factory, dmRender::GetFontMapUserData(font_map));
        }

        // This is a workaround, ideally we'd have a FontmapResource* // MAWE + JG
        Resources* font_map_resources           = new Resources;
        font_map_resources->m_MaterialResource  = material_res;
        font_map_resources->m_GlyphBankResource = glyph_bank_res;

        dmRender::SetFontMapUserData(font_map, (void*) font_map_resources);
        dmRender::SetFontMapMaterial(font_map, material_res->m_Material);

        dmDDF::FreeMessage(ddf);

        *font_map_out = font_map;
        return dmResource::RESULT_OK;
    }


    dmResource::Result ResFontMapPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapCreate(const dmResource::ResourceCreateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext) params.m_Context;
        dmRender::HFontMap font_map;

        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, render_context, ddf, 0, params.m_Filename, &font_map, false);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*)font_map;
            params.m_Resource->m_ResourceSize = dmRender::GetFontMapResourceSize(font_map);
        }
        else
        {
            params.m_Resource->m_Resource = 0;
        }
        return r;
    }

    dmResource::Result ResFontMapDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, dmRender::GetFontMapUserData(font_map));
        dmRender::DeleteFontMap(font_map);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)params.m_Resource->m_Resource;

        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params.m_Factory, (dmRender::HRenderContext) params.m_Context, ddf, font_map, params.m_Filename, &font_map, true);
        if(r != dmResource::RESULT_OK)
        {
            return r;
        }
        params.m_Resource->m_ResourceSize = dmRender::GetFontMapResourceSize(font_map);
        return dmResource::RESULT_OK;
    }
}
