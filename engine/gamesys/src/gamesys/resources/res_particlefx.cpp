// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_particlefx.h"

#include <dlib/log.h>

#include <particle/particle.h>
#include <particle/particle_ddf.h>
#include <render/render.h>
#include "res_material.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, dmParticle::HPrototype prototype, const char* filename)
    {
        if (prototype == dmParticle::INVALID_PROTOTYPE)
        {
            dmLogWarning("Particle fx could not be loaded: %s.", filename);
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result result;
        uint32_t emitter_count = dmParticle::GetEmitterCount(prototype);
        for (uint32_t i = 0; i < emitter_count; ++i)
        {
            void* tile_source;
            const char* tile_source_path = dmParticle::GetTileSourcePath(prototype, i);
            result = dmResource::Get(factory, tile_source_path, (void**) &tile_source);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Could not load texture \"%s\" for particle fx \"%s\".", tile_source_path, filename);
                return result;
            }
            dmParticle::SetTileSource(prototype, i, tile_source);
            MaterialResource* material;
            const char* material_path = dmParticle::GetMaterialPath(prototype, i);
            result = dmResource::Get(factory, material_path, (void**) &material);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Could not load material \"%s\" for particle fx \"%s\".", material_path, filename);
                return result;
            }
            dmParticle::SetMaterial(prototype, i, material);
            if(dmRender::GetMaterialVertexSpace(material->m_Material) != dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD)
            {
                dmLogError("Failed to create ParticleFX component. This component only supports materials with the Vertex Space property set to 'vertex-space-world'");
                return dmResource::RESULT_NOT_SUPPORTED;
            }
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
                {
                    dmResource::Release(factory, material);
                    dmParticle::SetMaterial(prototype, i, 0x0);
                }
                void* tile_source = dmParticle::GetTileSource(prototype, i);
                if (tile_source != 0)
                {
                    dmResource::Release(factory, tile_source);
                    dmParticle::SetTileSource(prototype, i, 0x0);
                }
            }
        }
    }


    dmResource::Result ResParticleFXPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmParticleDDF::ParticleFX* particle_fx;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmParticleDDF_ParticleFX_DESCRIPTOR, (void**) &particle_fx);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        for(uint32_t i = 0; i < particle_fx->m_Emitters.m_Count; ++i)
        {
            dmResource::PreloadHint(params->m_HintInfo, particle_fx->m_Emitters.m_Data[i].m_TileSource);
            if (particle_fx->m_Emitters.m_Data[i].m_Material[0] != 0)
                dmResource::PreloadHint(params->m_HintInfo, particle_fx->m_Emitters.m_Data[i].m_Material);
        }
        *params->m_PreloadData = particle_fx;
        return dmResource::RESULT_OK;
    }


    dmResource::Result ResParticleFXCreate(const dmResource::ResourceCreateParams* params)
    {
        if(params->m_PreloadData == 0)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        dmParticle::HPrototype prototype = dmParticle::NewPrototypeFromDDF((dmParticleDDF::ParticleFX *) params->m_PreloadData);
        dmResource::Result r = AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, prototype, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, prototype);
        }
        else
        {
            ReleasePrototypeResources(params->m_Factory, prototype);
            dmParticle::DeletePrototype(prototype);
        }
        return r;
    }

    dmResource::Result ResParticleFXDestroy(const dmResource::ResourceDestroyParams* params)
    {
        dmParticle::HPrototype prototype = (dmParticle::HPrototype)dmResource::GetResource(params->m_Resource);
        assert(prototype != dmParticle::INVALID_PROTOTYPE);
        ReleasePrototypeResources(params->m_Factory, prototype);
        dmParticle::DeletePrototype(prototype);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResParticleFXRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmParticle::HPrototype prototype = (dmParticle::HPrototype)dmResource::GetResource(params->m_Resource);
        ReleasePrototypeResources(params->m_Factory, prototype);
        if (!dmParticle::ReloadPrototype(prototype, params->m_Buffer, params->m_BufferSize))
        {
            return dmResource::RESULT_INVALID_DATA;
        }
        return AcquireResources(params->m_Factory, params->m_Buffer, params->m_BufferSize, prototype, params->m_Filename);
    }
}
