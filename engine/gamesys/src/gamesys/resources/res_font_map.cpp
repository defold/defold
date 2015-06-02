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
        dmRenderDDF::FontMap* ddf, dmRender::HFontMap font_map, const char* filename, dmRender::HFontMap* font_map_out)
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
            o_g.m_X = i_g.m_X;
            o_g.m_Y = i_g.m_Y;
        }
        params.m_TextureWidth = ddf->m_ImageWidth;
        params.m_TextureHeight = ddf->m_ImageHeight;
        params.m_TextureData = &ddf->m_ImageData[0];
        params.m_TextureDataSize = ddf->m_ImageData.m_Count;
        params.m_ShadowX = ddf->m_ShadowX;
        params.m_ShadowY = ddf->m_ShadowY;
        params.m_MaxAscent = ddf->m_MaxAscent;
        params.m_MaxDescent = ddf->m_MaxDescent;
        params.m_SdfOffset = ddf->m_SdfOffset;
        params.m_SdfScale = ddf->m_SdfScale;
        params.m_SdfOutline = ddf->m_SdfOutline;

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
        dmResource::Result r = AcquireResources(factory, render_context, ddf, 0, filename, &font_map);
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

        dmResource::Result r = AcquireResources(factory, (dmRender::HRenderContext)context, ddf, font_map, filename, &font_map);
        return r;
    }
}
