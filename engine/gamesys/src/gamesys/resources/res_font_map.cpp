#include "res_font_map.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font_renderer.h>
#include <render/font_ddf.h>
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
        params.m_SdfScale = ddf->m_SdfScale;
        params.m_SdfOutline = ddf->m_SdfOutline;

        params.m_CacheWidth = ddf->m_CacheWidth;
        params.m_CacheHeight = ddf->m_CacheHeight;
        params.m_GlyphDataSize = ddf->m_GlyphDataSize;

        params.m_GlyphChannels = ddf->m_GlyphChannels;

        params.m_CacheCellWidth = ddf->m_CacheCellWidth;
        params.m_CacheCellHeight = ddf->m_CacheCellHeight;
        params.m_CacheCellPadding = ddf->m_GlyphPadding;

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

        // free(params.m_GlyphData);
        dmDDF::FreeMessage(ddf);

        *font_map_out = font_map;
        return dmResource::RESULT_OK;
    }


    dmResource::Result ResFontMapPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void **preload_data,
                                     const char* filename)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(hint_info, ddf->m_Material);

        *preload_data = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void* preload_data,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        dmRender::HFontMap font_map;

        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) preload_data;
        dmResource::Result r = AcquireResources(factory, render_context, ddf, 0, filename, &font_map, false);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*)font_map;
        }
        else
        {
            resource->m_Resource = 0;
        }
        return r;
    }

    dmResource::Result ResFontMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)resource->m_Resource;
        dmResource::Release(factory, (void*)dmRender::GetFontMapMaterial(font_map));
        dmRender::DeleteFontMap(font_map);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)resource->m_Resource;

        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(factory, (dmRender::HRenderContext)context, ddf, font_map, filename, &font_map, true);
        return r;
    }
}
