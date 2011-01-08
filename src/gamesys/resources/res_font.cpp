#include "res_font.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <graphics/graphics_device.h>

#include <render/font_renderer.h>
#include <render/font_ddf.h>
#include <render/render_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResFontCreate(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRenderDDF::FontDesc* font_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontDesc>(buffer, buffer_size, &font_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        const char* base_dir_end = strrchr(filename, '.');
        if (base_dir_end == 0x0)
        {
            dmLogError("Could not find the file extension of %s.", filename);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        char font_path[256];
        uint32_t base_dir_size = base_dir_end - filename;
        dmStrlCpy(font_path, filename, base_dir_size + 2); // 2 for the terminating '.' and '\0'
        dmStrlCat(font_path, "imagefontc", 256);
        dmRender::HImageFont image_font;
        dmResource::Get(factory, font_path, (void**) &image_font);
        dmRender::HMaterial material;
        dmResource::Get(factory, font_desc->m_Material, (void**) &material);

        if (image_font == 0 || material == 0)
        {
            if (image_font) dmResource::Release(factory, image_font);
            if (material) dmResource::Release(factory, (void*) material);
            dmDDF::FreeMessage(font_desc);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmRender::HFont font = dmRender::NewFont(image_font);

        dmRender::SetMaterial(font, material);
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
        dmResource::Release(factory, (void*) dmRender::GetMaterial(font));

        dmRender::DeleteFont(font);

        return dmResource::CREATE_RESULT_OK;
    }
}
