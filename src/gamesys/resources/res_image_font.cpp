#include "res_image_font.h"

#include <render/fontrenderer.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResImageFontCreate(dmResource::HFactory factory,
                                          void* context,
                                          const void* buffer, uint32_t buffer_size,
                                          dmResource::SResourceDescriptor* resource,
                                          const char* filename)
    {
        dmRender::HImageFont font = dmRender::NewImageFont(buffer, buffer_size);
        if (font)
        {
            resource->m_Resource = (void*)font;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResImageFontDestroy(dmResource::HFactory factory,
                                           void* context,
                                           dmResource::SResourceDescriptor* resource)
    {
        dmRender::DeleteImageFont((dmRender::HImageFont) resource->m_Resource);

        return dmResource::CREATE_RESULT_OK;
    }
}
