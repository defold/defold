#include "res_emitter.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{
    static dmResource::FactoryResult LoadPrototypeResources(dmResource::HFactory factory, dmParticle::Prototype* prototype, const char* filename)
    {
        dmResource::FactoryResult result;
        result = dmResource::Get(factory, prototype->m_DDF->m_Texture.m_Name, (void**) &prototype->m_Texture);
        if (result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogError("Could not load texture \"%s\" for emitter \"%s\".", prototype->m_DDF->m_Texture.m_Name, filename);
        }
        if (result == dmResource::FACTORY_RESULT_OK)
        {
            result = dmResource::Get(factory, prototype->m_DDF->m_Material, (void**) &prototype->m_Material);
            if (result != dmResource::FACTORY_RESULT_OK)
            {
                dmLogError("Could not load material \"%s\" for emitter \"%s\".", prototype->m_DDF->m_Material, filename);
            }
        }
        return result;
    }

    static void ReleasePrototypeResources(dmResource::HFactory factory, dmParticle::Prototype* prototype)
    {
        if (prototype->m_Material != 0)
            dmResource::Release(factory, prototype->m_Material);
        if (prototype->m_Texture != 0)
            dmResource::Release(factory, prototype->m_Texture);
    }

    dmResource::CreateResult ResEmitterCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticleDDF::Emitter* prototype_ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmParticleDDF_Emitter_DESCRIPTOR, (void**) &prototype_ddf);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogWarning("Emitter could not be loaded: %s.", filename);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmParticle::Prototype* prototype = new dmParticle::Prototype();
        prototype->m_DDF = prototype_ddf;
        dmResource::FactoryResult result = LoadPrototypeResources(factory, prototype, filename);
        if (result != dmResource::FACTORY_RESULT_OK)
        {
            if (prototype != 0x0)
            {
                ReleasePrototypeResources(factory, prototype);
                if (prototype->m_DDF != 0x0)
                    dmDDF::FreeMessage((void*) prototype->m_DDF);
                delete prototype;
            }
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        resource->m_Resource = (void*) prototype;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResEmitterDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)resource->m_Resource;
        if (prototype != 0x0)
        {
            ReleasePrototypeResources(factory, prototype);
            if (prototype->m_DDF != 0x0)
                dmDDF::FreeMessage((void*) prototype->m_DDF);
            delete prototype;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResEmitterRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)resource->m_Resource;

        ReleasePrototypeResources(factory, prototype);
        if (prototype->m_DDF != 0x0)
            dmDDF::FreeMessage((void*) prototype->m_DDF);

        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmParticleDDF_Emitter_DESCRIPTOR, (void**) &prototype->m_DDF);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogWarning("Emitter could not be loaded: %s.", filename);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        if (LoadPrototypeResources(factory, prototype, filename) != dmResource::FACTORY_RESULT_OK)
        {
            ReleasePrototypeResources(factory, prototype);
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        return dmResource::CREATE_RESULT_OK;
    }
}
