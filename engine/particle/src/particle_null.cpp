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

#include "particle.h"

#include <string.h>

namespace dmGameSystem
{
    dmParticle::FetchResourcesResult FetchResourcesCallback(const dmParticle::FetchResourcesParams*, dmParticle::FetchResourcesData* out_data)
    {
        memset(out_data, 0, sizeof(*out_data));
        return dmParticle::FETCH_RESOURCES_NOT_FOUND;
    }
} // namespace dmGameSystem

namespace dmParticle
{
    struct Context
    {
    };

    HParticleContext CreateContext(uint32_t max_instance_count, uint32_t max_particle_count)
    {
        return new Context;
    }

    void DestroyContext(HParticleContext context)
    {
        delete context;
    }

    HInstance CreateInstance(HParticleContext context, HPrototype prototype, EmitterStateChangedData* data)
    {
        return INVALID_INSTANCE;
    }

    void DestroyInstance(HParticleContext context, HInstance instance)
    {
    }

    void StartInstance(HParticleContext context, HInstance instance)
    {
    }

    void StopInstance(HParticleContext context, HInstance instance, bool clear_particles)
    {
    }

    void SetPosition(HParticleContext context, HInstance instance, const dmVMath::Point3& position)
    {
    }

    void SetRotation(HParticleContext context, HInstance instance, const dmVMath::Quat& rotation)
    {
    }

    void SetScale(HParticleContext context, HInstance instance, float scale)
    {
    }

    bool IsSleeping(HParticleContext context, HInstance instance)
    {
        return true;
    }

    void Update(HParticleContext context, float dt, FetchResourcesCallback fetch_resources_callback)
    {
    }

    void UpdateRenderData(HParticleContext context, HInstance instance, uint32_t emitter_index, float dt)
    {
    }

    uint32_t GetEmitterVertexCount(HParticleContext context, HInstance instance, uint32_t emitter_index)
    {
        return 0;
    }

    GenerateVertexDataResult GenerateVertexData(HParticleContext                        context,
                                                HInstance                               instance,
                                                uint32_t                                emitter_index,
                                                const dmGraphics::VertexAttributeInfos& attribute_infos,
                                                const dmVMath::Vector4&                 color,
                                                void*                                   vertex_buffer,
                                                uint32_t                                vertex_buffer_size,
                                                uint32_t*                               out_vertex_buffer_size)
    {
        if (out_vertex_buffer_size)
            *out_vertex_buffer_size = 0;
        return GENERATE_VERTEX_DATA_INVALID_INSTANCE;
    }

    uint32_t GetInstanceEmitterCount(HParticleContext context, HInstance instance)
    {
        return 0;
    }

    void GetEmitterRenderData(HParticleContext context, HInstance instance, uint32_t emitter_index, EmitterRenderData** data)
    {
        if (data)
            *data = 0;
    }

    void SetRenderConstant(HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash, dmVMath::Vector4 value)
    {
    }

    void ResetRenderConstant(HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash)
    {
    }

    uint32_t GetVertexBufferSize(uint32_t particle_count, uint32_t vertex_size)
    {
        return 0;
    }
} // namespace dmParticle
