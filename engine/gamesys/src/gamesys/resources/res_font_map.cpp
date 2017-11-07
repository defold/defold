#include "res_font_map.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font_renderer.h>
#include <render/render_ddf.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, dmRender::HRenderContext context,
        dmRenderDDF::FontMap* ddf, dmRender::HFontMap font_map, const char* filename, dmRender::HFontMap* font_map_out, bool reload)
    {
        *font_map_out = 0;

        dmRender::HMaterial material;
        dmResource::Result result = dmResource::Get(factory, ddf->m_Material, (void**) &material);
        if (result != dmResource::RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return result;
        }

        dmRender::FontMapParams params;
        params.m_Glyphs.SetCapacity(ddf->m_Glyphs.m_Count);
        params.m_Glyphs.SetSize(ddf->m_Glyphs.m_Count);
        for (uint32_t i = 0; i < ddf->m_Glyphs.m_Count; ++i)
        {
            dmRenderDDF::FontMap::Glyph& i_g = ddf->m_Glyphs[i];
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
        params.m_ShadowX = ddf->m_ShadowX;
        params.m_ShadowY = ddf->m_ShadowY;
        params.m_MaxAscent = ddf->m_MaxAscent;
        params.m_MaxDescent = ddf->m_MaxDescent;
        params.m_SdfOffset = ddf->m_SdfOffset;
        params.m_SdfSpread = ddf->m_SdfSpread;
        params.m_SdfOutline = ddf->m_SdfOutline;
        params.m_OutlineAlpha = ddf->m_OutlineAlpha;
        params.m_ShadowAlpha = ddf->m_ShadowAlpha;
        params.m_Alpha = ddf->m_Alpha;

        params.m_CacheWidth = ddf->m_CacheWidth;
        params.m_CacheHeight = ddf->m_CacheHeight;

        params.m_GlyphChannels = ddf->m_GlyphChannels;

        params.m_CacheCellWidth = ddf->m_CacheCellWidth;
        params.m_CacheCellHeight = ddf->m_CacheCellHeight;
        params.m_CacheCellPadding = ddf->m_GlyphPadding;

        params.m_ImageFormat = ddf->m_ImageFormat;

        // Copy and unpack glyphdata
        params.m_GlyphData = malloc(ddf->m_GlyphData.m_Count);
        memcpy(params.m_GlyphData, ddf->m_GlyphData.m_Data, ddf->m_GlyphData.m_Count);

        if (font_map == 0)
            font_map = dmRender::NewFontMap(dmRender::GetGraphicsContext(context), params);
        else
        {
            dmRender::SetFontMap(font_map, params);
            dmResource::Release(factory, dmRender::GetFontMapMaterial(font_map));
        }

        dmRender::SetFontMapMaterial(font_map, material);

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
        dmResource::Release(params.m_Factory, (void*)dmRender::GetFontMapMaterial(font_map));
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
        return r;
    }
}
