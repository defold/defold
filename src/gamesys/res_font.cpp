#include "res_font.h"

#include <dlib/dstrings.h>

#include <graphics/graphics_device.h>

#include <render/fontrenderer.h>
#include <render/render_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFontCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRender::FontDesc* font_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmRender::FontDesc>(buffer, buffer_size, &font_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmRender::HImageFont image_font;
        dmGraphics::HVertexProgram vertex_program;
        dmGraphics::HFragmentProgram fragment_program;

        char vertex_program_buf[1024];
        DM_SNPRINTF(vertex_program_buf, sizeof(vertex_program_buf), "%s.arbvp", font_desc->m_VertexProgram);
        char fragment_program_buf[1024];
        DM_SNPRINTF(fragment_program_buf, sizeof(fragment_program_buf), "%s.arbfp", font_desc->m_FragmentProgram);

        dmResource::Get(factory, font_desc->m_Font, (void**) &image_font);
        dmResource::Get(factory, vertex_program_buf, (void**) &vertex_program);
        dmResource::Get(factory, fragment_program_buf, (void**) &fragment_program);

        if (image_font == 0 || vertex_program == 0 || fragment_program == 0)
        {
            if (image_font) dmResource::Release(factory, image_font);
            if (vertex_program) dmResource::Release(factory, (void*) vertex_program);
            if (fragment_program) dmResource::Release(factory, (void*) fragment_program);
            dmDDF::FreeMessage(font_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmRender::HFont font = dmRender::NewFont(image_font);

        dmRender::SetVertexProgram(font, vertex_program);
        dmRender::SetFragmentProgram(font, fragment_program);
        resource->m_Resource = (void*)font;
        dmDDF::FreeMessage(font_desc);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResFontDestroy(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmRender::HFont font = (dmRender::HFont) resource->m_Resource;
        dmResource::Release(factory, (void*) dmRender::GetImageFont(font));
        dmResource::Release(factory, (void*) dmRender::GetVertexProgram(font));
        dmResource::Release(factory, (void*) dmRender::GetFragmentProgram(font));

        dmRender::DeleteFont(font);

        return dmResource::CREATE_RESULT_OK;
    }
}
