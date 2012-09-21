#include "res_particlefx.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, dmParticle::Prototype* prototype, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmParticleDDF_Emitter_DESCRIPTOR, (void**) &prototype->m_DDF);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogWarning("Emitter could not be loaded: %s.", filename);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result result;
        result = dmResource::Get(factory, prototype->m_DDF->m_Texture.m_Name, (void**) &prototype->m_Texture);
        if (result != dmResource::RESULT_OK)
        {
            dmLogError("Could not load texture \"%s\" for emitter \"%s\".", prototype->m_DDF->m_Texture.m_Name, filename);
            return result;
        }
        if (result == dmResource::RESULT_OK)
        {
            result = dmResource::Get(factory, prototype->m_DDF->m_Material, (void**) &prototype->m_Material);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Could not load material \"%s\" for emitter \"%s\".", prototype->m_DDF->m_Material, filename);
                return result;
            }
        }
        return dmResource::RESULT_OK;
    }

    static void ReleasePrototypeResources(dmResource::HFactory factory, dmParticle::Prototype* prototype)
    {
        if (prototype->m_DDF != 0x0)
            dmDDF::FreeMessage((void*) prototype->m_DDF);
        if (prototype->m_Material != 0)
            dmResource::Release(factory, prototype->m_Material);
        if (prototype->m_Texture != 0)
            dmResource::Release(factory, prototype->m_Texture);
    }

    dmResource::Result ResParticleFXCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticle::Prototype* prototype = new dmParticle::Prototype();
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, prototype, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) prototype;
        }
        else
        {
            ReleasePrototypeResources(factory, prototype);
            delete prototype;
        }
        return r;
    }

    dmResource::Result ResParticleFXDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)resource->m_Resource;
        assert(prototype);
        ReleasePrototypeResources(factory, prototype);
        delete prototype;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResParticleFXRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)resource->m_Resource;
        dmParticle::Prototype tmp_prototype;
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, &tmp_prototype, filename);
        if (r == dmResource::RESULT_OK)
        {
            ReleasePrototypeResources(factory, prototype);
            *prototype = tmp_prototype;
        }
        else
        {
            ReleasePrototypeResources(factory, &tmp_prototype);
        }
        return r;
    }
}
