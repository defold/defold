#include "res_emitter.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{

    dmResource::Result ResEmitterCreate(const dmResource::ResourceCreateParams& params)
    {
        dmLogWarning("%s will not be loaded since emitter files are deprecated", params.m_Filename);
        // Trick resource system into thinking we loaded something
        params.m_Resource->m_Resource = (void*)1;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterDestroy(const dmResource::ResourceDestroyParams& params)
    {
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResEmitterRecreate(const dmResource::ResourceRecreateParams& params)
    {
        return dmResource::RESULT_OK;
    }
}
