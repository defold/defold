#include "res_emitter.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{

    dmResource::Result ResEmitterCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmLogWarning("%s will not be loaded since emitter files are deprecated", filename);
        // Trick resource system into thinking we loaded something
        resource->m_Resource = (void*)1;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        return dmResource::RESULT_OK;
    }
}
