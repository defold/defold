#include "res_font_map.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font_renderer.h>
#include <render/font_ddf.h>
#include <render/render_ddf.h>

namespace dmGameSystem
{
    dmRender::HFontMap AcquireResources(dmResource::HFactory factory, dmRender::HRenderContext context,
        const void* buffer, uint32_t buffer_size, dmRender::HFontMap font_map, const char* filename)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return 0;
        }

        dmRender::HMaterial material;
        dmResource::FactoryResult result = dmResource::Get(factory, ddf->m_Material, (void**) &material);
        if (result != dmResource::FACTORY_RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return 0;
        }

        dmRender::FontMapParams params;
        params.m_Glyphs.SetCapacity(ddf->m_Glyphs.m_Count);
        params.m_Glyphs.SetSize(ddf->m_Glyphs.m_Count);
        for (uint32_t i = 0; i < ddf->m_Glyphs.m_Count; ++i)
        {
            dmRenderDDF::FontMap::Glyph& i_g = ddf->m_Glyphs[i];
            dmRender::Glyph& o_g = params.m_Glyphs[i];
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

        if (font_map == 0)
            font_map = dmRender::NewFontMap(dmRender::GetGraphicsContext(context), params);
        else
        {
            dmRender::SetFontMap(font_map, params);
            dmResource::Release(factory, dmRender::GetFontMapMaterial(font_map));
        }

        dmRender::SetFontMapMaterial(font_map, material);

        dmDDF::FreeMessage(ddf);

        return font_map;
    }

    dmResource::CreateResult ResFontMapCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        dmRender::HFontMap font_map = AcquireResources(factory, render_context, buffer, buffer_size, 0, filename);
        if (font_map != 0)
        {
            resource->m_Resource = (void*)font_map;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResFontMapDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)resource->m_Resource;
        dmResource::Release(factory, (void*)dmRender::GetFontMapMaterial(font_map));
        dmRender::DeleteFontMap(font_map);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResFontMapRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        dmRender::HFontMap font_map = (dmRender::HFontMap)resource->m_Resource;
        if (AcquireResources(factory, (dmRender::HRenderContext)context, buffer, buffer_size, font_map, filename))
        {
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
