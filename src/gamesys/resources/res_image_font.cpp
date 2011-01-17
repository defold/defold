#include "res_image_font.h"

namespace dmGameSystem
{
    bool AcquireResources(const void* buffer, uint32_t buffer_size, ImageFontResource* resource)
    {
        resource->m_ImageFont = dmRender::NewImageFont(buffer, buffer_size);
        return resource->m_ImageFont != 0;
    }

    dmResource::CreateResult ResImageFontCreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        ImageFontResource* image_font = new ImageFontResource();
        image_font->m_ImageFont = 0;
        if (AcquireResources(buffer, buffer_size, image_font))
        {
            resource->m_Resource = (void*)image_font;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            delete image_font;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResImageFontDestroy(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource)
    {
        ImageFontResource* image_font = (ImageFontResource*)resource->m_Resource;
        dmRender::DeleteImageFont(image_font->m_ImageFont);
        delete image_font;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResImageFontRecreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        ImageFontResource* image_font = (ImageFontResource*)resource->m_Resource;
        ImageFontResource tmp_image_font;
        if (AcquireResources(buffer, buffer_size, &tmp_image_font))
        {
            dmRender::DeleteImageFont(image_font->m_ImageFont);
            *image_font = tmp_image_font;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }
}
