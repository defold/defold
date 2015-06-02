#include "res_anim.h"

namespace dmGameObject
{
    dmResource::Result ResAnimCreate(dmResource::HFactory factory,
                                       void* context,
                                       const void* buffer, uint32_t buffer_size,
                                       void* preload_data,
                                       dmResource::SResourceDescriptor* resource,
                                       const char* filename)
    {
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    dmResource::Result ResAnimDestroy(dmResource::HFactory factory,
                                        void* context,
                                        dmResource::SResourceDescriptor* resource)
    {
        return dmResource::RESULT_NOT_SUPPORTED;
    }
}
