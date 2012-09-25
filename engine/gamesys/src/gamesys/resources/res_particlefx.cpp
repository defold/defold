#include "res_particlefx.h"

#include <dlib/log.h>

#include <particle/particle_ddf.h>
#include <particle/particle.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, dmParticle::HPrototype* prototype, const char* filename)
    {
        *prototype = dmParticle::NewPrototype(buffer, buffer_size);
        if (*prototype == dmParticle::INVALID_PROTOTYPE)
        {
            dmLogWarning("Particle fx could not be loaded: %s.", filename);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result result;
        uint32_t emitter_count = dmParticle::GetEmitterCount(*prototype);
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            void* texture;
            const char* texture_path = dmParticle::GetTexturePath(*prototype, i);
            result = dmResource::Get(factory, texture_path, (void**) &texture);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Could not load texture \"%s\" for particle fx \"%s\".", texture_path, filename);
                return result;
            }
            dmParticle::SetTexture(*prototype, i, texture);
            void* material;
            const char* material_path = dmParticle::GetMaterialPath(*prototype, i);
            result = dmResource::Get(factory, material_path, (void**) &material);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Could not load material \"%s\" for particle fx \"%s\".", material_path, filename);
                return result;
            }
            dmParticle::SetMaterial(*prototype, i, material);
        }
        return dmResource::RESULT_OK;
    }

    static void ReleasePrototypeResources(dmResource::HFactory factory, dmParticle::HPrototype prototype)
    {
        if (prototype != dmParticle::INVALID_PROTOTYPE)
        {
            uint32_t emitter_count = dmParticle::GetEmitterCount(prototype);
            for (uint32_t i = 0; i < emitter_count; ++i)
            {
                void* material = dmParticle::GetMaterial(prototype, i);
                if (material != 0)
                    dmResource::Release(factory, material);
                void* texture = dmParticle::GetTexture(prototype, i);
                if (texture != 0)
                    dmResource::Release(factory, texture);
            }
            dmParticle::DeletePrototype(prototype);
        }
    }

    dmResource::Result ResParticleFXCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticle::HPrototype prototype = dmParticle::INVALID_PROTOTYPE;
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, &prototype, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) prototype;
        }
        else
        {
            ReleasePrototypeResources(factory, prototype);
        }
        return r;
    }

    dmResource::Result ResParticleFXDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmParticle::HPrototype prototype = (dmParticle::HPrototype)resource->m_Resource;
        assert(prototype != dmParticle::INVALID_PROTOTYPE);
        ReleasePrototypeResources(factory, prototype);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResParticleFXRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmParticle::HPrototype prototype = (dmParticle::HPrototype)resource->m_Resource;
        dmParticle::HPrototype tmp_prototype;
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, &tmp_prototype, filename);
        if (r == dmResource::RESULT_OK)
        {
            ReleasePrototypeResources(factory, prototype);
            resource->m_Resource = tmp_prototype;
        }
        else
        {
            ReleasePrototypeResources(factory, tmp_prototype);
        }
        return r;
    }
}
