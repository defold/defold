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

#include "comp_particlefx.h"

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dmsdk/dlib/vmath.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"

#include "resources/res_particlefx.h"
#include "resources/res_textureset.h"
#include "resources/res_material.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_ParticleFx, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_ParticleVertexCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# vertices", &rmtp_ParticleFx);
DM_PROPERTY_U32(rmtp_ParticleVertexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of CPU vertex buffer (in bytes)", &rmtp_ParticleFx);
DM_PROPERTY_U32(rmtp_ParticleVertexSizeGPU, 0, PROFILE_PROPERTY_FRAME_RESET, "size of GPU vertex buffer (in bytes)", &rmtp_ParticleFx);

namespace dmGameSystem
{
    const int VERTEX_COUNT = 6; // Fixed vertex count per particle

    using namespace dmVMath;

    struct ParticleFXWorld;

    struct ParticleFXComponentPrototype
    {
        Vector3 m_Translation;
        Quat m_Rotation;
        dmParticle::HPrototype m_ParticlePrototype;
        uint16_t m_AddedToUpdate : 1;
        uint16_t : 15;
    };

    struct ParticleFXComponent
    {
        dmGameObject::HInstance m_Instance;
        dmhash_t m_ComponentId;
        dmParticle::HInstance m_ParticleInstance;
        dmParticle::HPrototype m_ParticlePrototype;
        ParticleFXWorld* m_World;
        uint32_t m_PrototypeIndex;
        uint16_t m_AddedToUpdate : 1;
        uint16_t : 15;
    };

    struct ParticleFXWorld
    {
        dmArray<ParticleFXComponent>            m_Components;
        dmArray<dmRender::RenderObject>         m_RenderObjects;
        dmArray<dmRender::HNamedConstantBuffer> m_ConstantBuffers;
        dmArray<ParticleFXComponentPrototype>   m_Prototypes;
        dmIndexPool32                           m_PrototypeIndices;
        ParticleFXContext*                      m_Context;
        dmParticle::HParticleContext            m_ParticleContext;
        dmRender::HBufferedRenderBuffer         m_VertexBuffer;
        dmArray<uint8_t>                        m_VertexBufferData;
        uint32_t                                m_VerticesWritten;
        uint32_t                                m_EmitterCount;
        uint32_t                                m_DispatchCount;
        uint32_t                                m_VertexBufferSize;
        uint32_t                                m_VertexBufferOffset; // Current write position
        float                                   m_DT;
        uint32_t                                m_WarnOutOfROs : 1;
        uint32_t                                m_WarnParticlesExceeded : 1;
    };

    dmGameObject::CreateResult CompParticleFXNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        assert(params.m_Context);

        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* world = new ParticleFXWorld();
        world->m_Context = ctx;
        uint32_t particle_fx_count = dmMath::Min(params.m_MaxComponentInstances, ctx->m_MaxParticleFXCount);
        world->m_ParticleContext = dmParticle::CreateContext(ctx->m_MaxParticleFXCount, ctx->m_MaxParticleCount);
        world->m_Components.SetCapacity(particle_fx_count);
        world->m_Prototypes.SetCapacity(particle_fx_count);
        world->m_Prototypes.SetSize(particle_fx_count);
        world->m_PrototypeIndices.SetCapacity(particle_fx_count);

        uint16_t max_emitter_count = ctx->m_MaxEmitterCount;
        world->m_RenderObjects.SetCapacity(max_emitter_count);
        world->m_ConstantBuffers.SetCapacity(max_emitter_count);
        world->m_ConstantBuffers.SetSize(max_emitter_count);
        memset(world->m_ConstantBuffers.Begin(), 0, sizeof(dmRender::HNamedConstantBuffer)*max_emitter_count);

        // position   : 3
        // color      : 4
        // texcoord0  : 2
        // page_index : 1
        const uint32_t particle_buffer_count = dmMath::Min(ctx->m_MaxParticleBufferCount, ctx->m_MaxParticleCount);
        const uint32_t default_vx_size       = sizeof(float) * (3 + 4 + 2 + 1);
        const uint32_t buffer_size           = particle_buffer_count * VERTEX_COUNT * default_vx_size;
        world->m_VertexBufferData.SetCapacity(buffer_size);
        world->m_VertexBufferData.SetSize(buffer_size);
        world->m_VertexBuffer = dmRender::NewBufferedRenderBuffer(ctx->m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
        world->m_VertexBufferSize = 0;

        world->m_WarnOutOfROs = 0;
        world->m_EmitterCount = 0;

        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* pfx_world = (ParticleFXWorld*)params.m_World;
        for (uint32_t i = 0; i < pfx_world->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &pfx_world->m_Components[i];
            dmResource::Release(pfx_world->m_Context->m_Factory, c->m_ParticlePrototype);
            dmParticle::DestroyInstance(pfx_world->m_ParticleContext, c->m_ParticleInstance);
        }

        for (uint32_t i = 0; i < pfx_world->m_ConstantBuffers.Size(); ++i)
        {
            if (pfx_world->m_ConstantBuffers[i])
            {
                dmRender::DeleteNamedConstantBuffer(pfx_world->m_ConstantBuffers[i]);
            }
        }

        dmParticle::DestroyContext(pfx_world->m_ParticleContext);
        dmRender::DeleteBufferedRenderBuffer(ctx->m_RenderContext, pfx_world->m_VertexBuffer);

        delete pfx_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*)params.m_World;
        if (world->m_PrototypeIndices.Remaining() == 0)
        {
            ShowFullBufferError("ParticleFx", dmParticle::MAX_INSTANCE_COUNT_KEY, world->m_PrototypeIndices.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_PrototypeIndices.Pop();
        ParticleFXComponentPrototype* prototype = &world->m_Prototypes[index];
        prototype->m_Translation = Vector3(params.m_Position);
        prototype->m_Rotation = params.m_Rotation;
        prototype->m_ParticlePrototype = (dmParticle::HPrototype)params.m_Resource;
        prototype->m_AddedToUpdate = false;
        *params.m_UserData = (uintptr_t)prototype;
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompParticleFXGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (void*)params.m_UserData;
    }

    dmGameObject::CreateResult CompParticleFXDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
        uint32_t index = prototype - w->m_Prototypes.Begin();
        for (uint32_t i = 0; i < w->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &w->m_Components[i];
            if (c->m_Instance == params.m_Instance && c->m_PrototypeIndex == index)
            {
                c->m_Instance = 0;
                dmParticle::RetireInstance(w->m_ParticleContext, c->m_ParticleInstance);
            }
        }
        w->m_PrototypeIndices.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void SetBlendFactors(dmRender::RenderObject* ro, dmParticleDDF::BlendMode blend_mode);
    static void SetRenderConstants(dmRender::HNamedConstantBuffer constant_buffer, dmParticle::RenderConstant* constants, uint32_t constant_count);
    void RenderLineCallback(void* usercontext, const dmVMath::Point3& start, const dmVMath::Point3& end, const dmVMath::Vector4& color);
    dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data);

    dmGameObject::CreateResult CompParticleFXAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
        prototype->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        ParticleFXWorld* w   = (ParticleFXWorld*)params.m_World;
        w->m_DT              = params.m_UpdateContext->m_DT;
        w->m_VerticesWritten = 0;
        w->m_DispatchCount   = 0;

        dmArray<ParticleFXComponent>& components = w->m_Components;
        if (components.Empty())
            return dmGameObject::UPDATE_RESULT_OK;

        dmParticle::HParticleContext particle_context = w->m_ParticleContext;
        uint32_t count = components.Size();

        // Update transforms
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent& c = w->m_Components[i];
            if (c.m_Instance != 0)
            {
                ParticleFXComponentPrototype* prototype = &w->m_Prototypes[c.m_PrototypeIndex];
                dmTransform::Transform world_transform(prototype->m_Translation, prototype->m_Rotation, 1.0f);
                world_transform = dmTransform::Mul(dmGameObject::GetWorldTransform(c.m_Instance), world_transform);
                dmParticle::SetPosition(particle_context, c.m_ParticleInstance, Point3(world_transform.GetTranslation()));
                dmParticle::SetRotation(particle_context, c.m_ParticleInstance, world_transform.GetRotation());
                dmParticle::SetScale(particle_context, c.m_ParticleInstance, world_transform.GetUniformScale());
                if (prototype->m_AddedToUpdate && !c.m_AddedToUpdate) {
                    dmParticle::StartInstance(particle_context, c.m_ParticleInstance);
                    c.m_AddedToUpdate = true;
                }
            }
        }

        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        dmParticle::Update(particle_context, params.m_UpdateContext->m_DT, FetchAnimationCallback);

        // Prune sleeping instances
        uint32_t i = 0;
        while (i < count)
        {
            ParticleFXComponent& c = components[i];
            if ((c.m_AddedToUpdate || c.m_Instance == 0) && dmParticle::IsSleeping(particle_context, c.m_ParticleInstance))
            {
                uint32_t emitter_count = dmParticle::GetEmitterCount(c.m_ParticlePrototype);
                w->m_EmitterCount -= emitter_count;
                dmResource::Release(ctx->m_Factory, c.m_ParticlePrototype);
                dmParticle::DestroyInstance(particle_context, c.m_ParticleInstance);
                components.EraseSwap(i);
                --count;
            }
            else
            {
                ++i;
            }
        }

        dmRender::TrimBuffer(ctx->m_RenderContext, w->m_VertexBuffer);
        dmRender::RewindBuffer(ctx->m_RenderContext, w->m_VertexBuffer);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static inline dmRender::HMaterial GetComponentMaterial(const dmParticle::EmitterRenderData* rd)
    {
        dmGameSystem::MaterialResource* material_res = (dmGameSystem::MaterialResource*) rd->m_Material;
        return material_res->m_Material;
    }

    static inline dmRender::HMaterial GetRenderMaterial(dmRender::HRenderContext render_context, const dmParticle::EmitterRenderData* rd)
    {
        dmRender::HMaterial context_material = dmRender::GetContextMaterial(render_context);
        return context_material ? context_material : GetComponentMaterial(rd);
    }

    static void RenderBatch(ParticleFXWorld* pfx_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("ParticleRenderBatch");
        const dmParticle::EmitterRenderData* first = (dmParticle::EmitterRenderData*) buf[*begin].m_UserData;
        ParticleFXContext* pfx_context = pfx_world->m_Context;
        dmParticle::HParticleContext particle_context = pfx_world->m_ParticleContext;

        dmRender::HMaterial material = GetRenderMaterial(render_context, first);
        dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material);

        dmGraphics::VertexAttributeInfos emitter_attribute_info = {};
        dmGraphics::VertexAttributeInfos material_attribute_info;
        // Same default coordinate space as the editor
        FillMaterialAttributeInfos(material, vx_decl, &material_attribute_info, dmGraphics::COORDINATE_SPACE_WORLD);

        const uint32_t vx_stride = material_attribute_info.m_VertexStride;
        const uint32_t max_gpu_count = pfx_context->m_MaxParticleCount;
        const uint32_t max_cpu_count = pfx_context->m_MaxParticleBufferCount; // How many particles will fit into the scratch buffer
        const uint32_t max_gpu_size = pfx_world->m_VertexBufferSize;
        const uint32_t max_cpu_size = dmMath::Min(max_cpu_count, max_gpu_count) * VERTEX_COUNT * vx_stride;

        // Each batch uses the scratch data exclusively (i.e. no mixed vertex formats)
        dmArray<uint8_t>& vertex_buffer = pfx_world->m_VertexBufferData;
        if (vertex_buffer.Capacity() < max_cpu_size)
        {
            vertex_buffer.SetCapacity(max_cpu_size);
            vertex_buffer.SetSize(max_cpu_size);
        }

        // Since we mix vertex formats, we need to align the write offset to the current stride
        if (pfx_world->m_VertexBufferOffset % vx_stride != 0)
        {
            pfx_world->m_VertexBufferOffset += vx_stride - pfx_world->m_VertexBufferOffset % vx_stride;
        }

        const uint32_t gpu_vb_offset_start= pfx_world->m_VertexBufferOffset;
        const uint32_t vertex_offset      = gpu_vb_offset_start / vx_stride;            // Offset in #particles
        const uint32_t vb_max_size        = pfx_world->m_VertexBufferData.Capacity();

        // loop vars
        uint32_t vb_size        = 0;
        uint32_t gpu_vb_offset  = gpu_vb_offset_start;   // The offset into the GPU vertex buffer

        for (uint32_t *i = begin; gpu_vb_offset < max_gpu_size && i != end; ++i)
        {
            const dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*) buf[*i].m_UserData;

            FillAttributeInfos(0, INVALID_DYNAMIC_ATTRIBUTE_INDEX, // Not supported yet
                    emitter_render_data->m_Attributes,
                    emitter_render_data->m_AttributeCount,
                    &material_attribute_info,
                    &emitter_attribute_info);

            uint32_t particle_count = dmParticle::GetParticleCount(particle_context, emitter_render_data->m_Instance, emitter_render_data->m_EmitterIndex);

            // fill up the vertex buffer, and then schedule an upload of vertex buffer data
            for (int p = 0; p < particle_count; )
            {
                uint32_t size_left = vb_max_size - vb_size;
                // only get the number of particles that will fit
                uint32_t num_particles_to_write = size_left / (VERTEX_COUNT * vx_stride);

                dmParticle::GenerateVertexDataResult res = dmParticle::GenerateVertexDataPartial(particle_context,
                    pfx_world->m_DT, emitter_render_data->m_Instance, emitter_render_data->m_EmitterIndex,
                    p, num_particles_to_write,
                    emitter_attribute_info, Vector4(1,1,1,1), (void*) vertex_buffer.Begin(), vb_max_size, &vb_size);

                // if the buffer is full
                // or if we couldn't fit a single particle
                bool flush = vb_size >= vb_max_size || num_particles_to_write == 0;

                // If the buffer didn't actually hold all particles, it's time to reset the buffer now
                if (res == dmParticle::GENERATE_VERTEX_DATA_MAX_PARTICLES_EXCEEDED)
                {
                    // If the buffer didn't actually hold all particles, it's time to reset the buffer now
                    flush = true;
                }
                else if (res == dmParticle::GENERATE_VERTEX_DATA_INVALID_INSTANCE)
                {
                    dmLogWarning("Cannot generate vertex data for emitter (%d), particle instance handle is invalid.", (*i));
                }

                p += num_particles_to_write;

                if (flush) // Upload the written data (if there was any)
                {
                    uint32_t vb_upload_size = vb_size;
                    if ((gpu_vb_offset + vb_upload_size) > max_gpu_size) // Make sure we don't do an overrun on the gpu buffer
                    {
                        vb_upload_size = max_gpu_size - gpu_vb_offset;
                        vb_upload_size = vb_upload_size - vb_upload_size % (VERTEX_COUNT * vx_stride); // Only upload vertices for a full particle
                    }

                    dmRender::SetBufferSubData(render_context, pfx_world->m_VertexBuffer, gpu_vb_offset, vb_upload_size, vertex_buffer.Begin());
                    gpu_vb_offset += vb_upload_size;
                    vb_size = 0;
                }
            }
        }

        if (vb_size != 0) // Do we have any lingering data?
        {
            uint32_t vb_upload_size = vb_size;
            if ((gpu_vb_offset + vb_upload_size) > max_gpu_size) // Make sure we don't do an overrun on the gpu buffer
            {
                vb_upload_size = max_gpu_size - gpu_vb_offset;
                vb_upload_size = vb_upload_size - vb_upload_size % (VERTEX_COUNT * vx_stride); // Only upload vertices for a full particle
            }

            dmRender::SetBufferSubData(render_context, pfx_world->m_VertexBuffer, gpu_vb_offset, vb_upload_size, vertex_buffer.Begin());
            gpu_vb_offset += vb_upload_size;
        }

        // In place writing of render object
        uint32_t ro_index = pfx_world->m_RenderObjects.Size();
        pfx_world->m_RenderObjects.SetSize(ro_index+1);

        TextureResource* texture_res = (TextureResource*) first->m_Texture;
        dmGraphics::HTexture texture = texture_res ? texture_res->m_Texture : 0;

        uint32_t num_vertices_written = (gpu_vb_offset - gpu_vb_offset_start) / vx_stride;

        dmRender::RenderObject& ro = pfx_world->m_RenderObjects[ro_index];
        ro.Init();
        ro.m_Material          = GetComponentMaterial(first);
        ro.m_VertexDeclaration = dmRender::GetVertexDeclaration(material);
        ro.m_Textures[0]       = texture;
        ro.m_VertexStart       = vertex_offset;
        ro.m_VertexCount       = num_vertices_written;
        ro.m_VertexBuffer      = (dmGraphics::HVertexBuffer) dmRender::GetBuffer(render_context, pfx_world->m_VertexBuffer);
        ro.m_PrimitiveType     = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_SetBlendFactors   = 1;

        SetBlendFactors(&ro, first->m_BlendMode);

        if (!pfx_world->m_ConstantBuffers[ro_index])
        {
            pfx_world->m_ConstantBuffers[ro_index] = dmRender::NewNamedConstantBuffer();
        }
        ro.m_ConstantBuffer = pfx_world->m_ConstantBuffers[ro_index];

        dmRender::ClearNamedConstantBuffer(ro.m_ConstantBuffer);
        SetRenderConstants(ro.m_ConstantBuffer, first->m_RenderConstants, first->m_RenderConstantsSize);

        dmRender::AddToRender(render_context, &ro);

        pfx_world->m_VertexBufferOffset = gpu_vb_offset;
        pfx_world->m_VerticesWritten += ro.m_VertexCount;
    }

    static uint32_t CalcVertexBufferSize(ParticleFXWorld* pfx_world, dmRender::HRenderContext render_context)
    {
        ParticleFXContext* pfx_context = pfx_world->m_Context;
        dmParticle::HParticleContext particle_context = pfx_world->m_ParticleContext;
        dmArray<ParticleFXComponent>& components = pfx_world->m_Components;
        uint32_t count = components.Size();

        uint32_t particle_count = 0;
        uint32_t buffer_size = 0;

        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent& c = pfx_world->m_Components[i];
            if (c.m_AddedToUpdate)
            {
                uint32_t emitter_count = dmParticle::GetEmitterCount(c.m_ParticlePrototype);
                for (uint32_t j = 0; j < emitter_count; ++j)
                {
                    dmParticle::EmitterRenderData* render_data;
                    dmParticle::GetEmitterRenderData(particle_context, c.m_ParticleInstance, j, &render_data);

                    dmRender::HMaterial mat = GetRenderMaterial(render_context, render_data);
                    dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(mat);
                    uint32_t stride = dmGraphics::GetVertexDeclarationStride(vx_decl);

                    uint32_t pcount = dmParticle::GetParticleCount(pfx_world->m_ParticleContext, render_data->m_Instance, render_data->m_EmitterIndex);

                    bool is_full = false;
                    if ((pcount + particle_count) > pfx_context->m_MaxParticleCount)
                    {
                        pcount = pfx_context->m_MaxParticleCount - particle_count;
                        is_full = true;

                        if (!pfx_world->m_WarnParticlesExceeded) {
                            dmLogWarning("Maximum number of particles (%d) exceeded, particles will not be rendered. Change \"%s\" in the config file.",
                                         pfx_context->m_MaxParticleCount, dmParticle::MAX_PARTICLE_GPU_COUNT_KEY);
                            pfx_world->m_WarnParticlesExceeded = 1;
                        }
                    }

                    particle_count += pcount;

                    // To accomodate for aligning the buffer to the different strides
                    // we add one extra particle to give us some extra room
                    ++pcount;

                    buffer_size += stride * pcount * VERTEX_COUNT;

                    if (is_full)
                        return buffer_size;
                }
            }
        }

        return buffer_size;
    }

    static void UpdateVertexBufferSize(ParticleFXWorld* pfx_world, dmRender::HRenderContext render_context)
    {
        uint32_t buffer_size = CalcVertexBufferSize(pfx_world, render_context);
        if (buffer_size > pfx_world->m_VertexBufferSize)
        {
            pfx_world->m_VertexBufferSize = buffer_size;
        }

        dmRender::SetBufferData(render_context, pfx_world->m_VertexBuffer, pfx_world->m_VertexBufferSize, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        ParticleFXWorld* pfx_world = (ParticleFXWorld*)params.m_UserData;
        switch(params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                pfx_world->m_VertexBufferOffset = 0;
                pfx_world->m_RenderObjects.SetSize(0);

                if (dmRender::GetBufferIndex(params.m_Context, pfx_world->m_VertexBuffer) < pfx_world->m_DispatchCount)
                {
                    dmRender::AddRenderBuffer(params.m_Context, pfx_world->m_VertexBuffer);
                }

                UpdateVertexBufferSize(pfx_world, params.m_Context);
                break;
            case dmRender::RENDER_LIST_OPERATION_BATCH:
                RenderBatch(pfx_world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                if (pfx_world->m_VertexBufferOffset)
                {
                    DM_PROPERTY_ADD_U32(rmtp_ParticleVertexCount, pfx_world->m_VerticesWritten);
                    DM_PROPERTY_ADD_U32(rmtp_ParticleVertexSize, pfx_world->m_VertexBufferData.Capacity());
                    DM_PROPERTY_ADD_U32(rmtp_ParticleVertexSizeGPU, pfx_world->m_VertexBufferSize);
                    pfx_world->m_DispatchCount++;
                }
                break;
            default:break;
        }
    }

    dmGameObject::UpdateResult CompParticleFXRender(const dmGameObject::ComponentsRenderParams& params)
    {
        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* pfx_world = (ParticleFXWorld*)params.m_World;
        dmParticle::HParticleContext particle_context = pfx_world->m_ParticleContext;
        dmArray<ParticleFXComponent>& components = pfx_world->m_Components;

        uint32_t count = components.Size();
        uint32_t world_emitter_count = pfx_world->m_EmitterCount;

        if (pfx_world->m_RenderObjects.Capacity() < world_emitter_count)
        {
            dmLogWarning("Max number of emitters reached (%u), some objects will not be rendered. Increase the capacity with particle_fx.max_emitter_count", pfx_world->m_RenderObjects.Capacity());
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        if (ctx->m_Debug)
        {
            dmParticle::DebugRender(particle_context, ctx->m_RenderContext, RenderLineCallback);
        }

        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(ctx->m_RenderContext, world_emitter_count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(ctx->m_RenderContext, &RenderListDispatch, pfx_world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent& c = pfx_world->m_Components[i];
            if (c.m_AddedToUpdate)
            {
                DM_PROPERTY_ADD_U32(rmtp_ParticleFx, 1);

                uint32_t emitter_count = dmParticle::GetEmitterCount(c.m_ParticlePrototype);
                for (uint32_t j = 0; j < emitter_count; ++j)
                {
                    dmParticle::EmitterRenderData* render_data;
                    dmParticle::GetEmitterRenderData(particle_context, c.m_ParticleInstance, j, &render_data);

                    dmGameSystem::MaterialResource* material_res = (dmGameSystem::MaterialResource*) render_data->m_Material;

                    write_ptr->m_WorldPosition = Point3(render_data->m_Transform.getTranslation());
                    write_ptr->m_UserData = (uintptr_t) render_data;
                    write_ptr->m_BatchKey = render_data->m_MixedHash;
                    write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(material_res->m_Material);
                    write_ptr->m_Dispatch = dispatch;
                    write_ptr->m_MinorOrder = 0;
                    write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
                    ++write_ptr;
                }
            }
        }

        dmRender::RenderListSubmit(ctx->m_RenderContext, render_list, write_ptr);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmParticle::HInstance CreateComponent(ParticleFXWorld* world, dmGameObject::HInstance go_instance, dmhash_t component_id, ParticleFXComponentPrototype* prototype, dmParticle::EmitterStateChangedData* emitter_state_changed_data)
    {
        if (world->m_Components.Full())
        {
            world->m_Components.OffsetCapacity(4);
        }

        uint32_t count = world->m_Components.Size();
        world->m_Components.SetSize(count + 1);
        ParticleFXComponent* component = &world->m_Components[count];
        component->m_Instance = go_instance;
        component->m_ComponentId = component_id;
        component->m_PrototypeIndex = prototype - world->m_Prototypes.Begin();
        // NOTE: We must increase ref-count as a particle fx might be playing after the component is destroyed
        dmResource::HFactory factory = world->m_Context->m_Factory;
        dmResource::IncRef(factory, prototype->m_ParticlePrototype);
        component->m_ParticleInstance = dmParticle::CreateInstance(world->m_ParticleContext, prototype->m_ParticlePrototype, emitter_state_changed_data);
        component->m_ParticlePrototype = prototype->m_ParticlePrototype;
        component->m_World = world;
        component->m_AddedToUpdate = prototype->m_AddedToUpdate;
        world->m_EmitterCount += dmParticle::GetEmitterCount(component->m_ParticlePrototype);
        return component->m_ParticleInstance;
    }

    dmGameObject::UpdateResult CompParticleFXOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*)params.m_World;
        if (params.m_Message->m_Id == dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash)
        {
            dmParticle::HParticleContext particle_context = world->m_ParticleContext;
            ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
            // NOTE: We make a stack allocated instance of EmitterStateChangedData here and pass the address on to create the particle instance.
            // dmParticle::CreateInstance can be called from outside this method (when reloading particlefx for example, where we don't care about callbacks)
            // so we want to be able to pass 0x0 in that case. If there is a callback present we will make a shallow copy of the pointers to the callback and userdata,
            // and transfer ownership of that memory to the particle instance.
            dmParticle::EmitterStateChangedData emitter_state_changed_data;
            if(params.m_Message->m_DataSize == sizeof(dmParticle::EmitterStateChanged) + sizeof(EmitterStateChangedScriptData))
            {
                emitter_state_changed_data.m_UserData = malloc(sizeof(EmitterStateChangedScriptData));
                memcpy(&(emitter_state_changed_data.m_StateChangedCallback), (params.m_Message->m_Data), sizeof(dmParticle::EmitterStateChanged));
                memcpy(emitter_state_changed_data.m_UserData, (params.m_Message->m_Data) + sizeof(dmParticle::EmitterStateChanged), sizeof(EmitterStateChangedScriptData));
            }

            dmhash_t component_id = params.m_Message->m_Receiver.m_Fragment;
            dmParticle::HInstance instance = CreateComponent(world, params.m_Instance, component_id, prototype, &emitter_state_changed_data);

            dmTransform::Transform world_transform(prototype->m_Translation, prototype->m_Rotation, 1.0f);
            world_transform = dmTransform::Mul(dmGameObject::GetWorldTransform(params.m_Instance), world_transform);
            dmParticle::SetPosition(particle_context, instance, Point3(world_transform.GetTranslation()));
            dmParticle::SetRotation(particle_context, instance, world_transform.GetRotation());
            dmParticle::SetScale(particle_context, instance, world_transform.GetUniformScale());

            if (prototype->m_AddedToUpdate)
            {
                dmParticle::StartInstance(particle_context, instance);
            }
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::StopParticleFX* ddf = (dmGameSystemDDF::StopParticleFX*)params.m_Message->m_Data;
            uint32_t count = world->m_Components.Size();
            for (uint32_t i = 0; i < count; ++i)
            {
                ParticleFXComponent* component = &world->m_Components[i];
                dmhash_t component_id = params.m_Message->m_Receiver.m_Fragment;
                if (component->m_Instance == params.m_Instance && component->m_ComponentId == component_id)
                {
                    dmParticle::StopInstance(world->m_ParticleContext, component->m_ParticleInstance, ddf->m_ClearParticles);
                }
            }
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::SetConstantParticleFX* ddf = (dmGameSystemDDF::SetConstantParticleFX*)params.m_Message->m_Data;
            uint32_t count = world->m_Components.Size();
            uint32_t found_count = 0;
            for (uint32_t i = 0; i < count; ++i)
            {
                ParticleFXComponent* component = &world->m_Components[i];
                if (component->m_Instance == params.m_Instance)
                {
                    if (ddf->m_IsMatrix4)
                    {
                        dmParticle::SetRenderConstantM4(world->m_ParticleContext, component->m_ParticleInstance, ddf->m_EmitterId, ddf->m_NameHash, ddf->m_Value);
                    }
                    else
                    {
                        dmParticle::SetRenderConstant(world->m_ParticleContext, component->m_ParticleInstance, ddf->m_EmitterId, ddf->m_NameHash, ddf->m_Value.getCol0());
                    }
                    ++found_count;
                }
            }
            if (found_count == 0)
            {
                dmLogWarning("Particle FX to set constant for could not be found. You need to start playing it before setting constants.");
            }
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor->m_NameHash)
        {
            dmGameSystemDDF::ResetConstantParticleFX* ddf = (dmGameSystemDDF::ResetConstantParticleFX*)params.m_Message->m_Data;
            uint32_t count = world->m_Components.Size();
            uint32_t found_count = 0;
            for (uint32_t i = 0; i < count; ++i)
            {
                ParticleFXComponent* component = &world->m_Components[i];
                if (component->m_Instance == params.m_Instance)
                {
                    dmParticle::ResetRenderConstant(world->m_ParticleContext, component->m_ParticleInstance, ddf->m_EmitterId, ddf->m_NameHash);
                    ++found_count;
                }
            }
            if (found_count == 0)
            {
                dmLogWarning("Particle FX to reset constant for could not be found.");
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompParticleFXOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*)params.m_World;
        world->m_WarnOutOfROs = 0;
        world->m_EmitterCount = 0;
        uint32_t count = world->m_Components.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent* component = &world->m_Components[i];
            if (component->m_ParticlePrototype == params.m_Resource)
            {
                dmParticle::ReloadInstance(world->m_ParticleContext, component->m_ParticleInstance, true);
            }
            world->m_EmitterCount += dmParticle::GetEmitterCount(component->m_ParticlePrototype);
        }
        // Don't warn if none could be found
    }

    static void SetBlendFactors(dmRender::RenderObject* ro, dmParticleDDF::BlendMode blend_mode)
    {
        switch (blend_mode)
        {
            case dmParticleDDF::BLEND_MODE_ALPHA:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmParticleDDF::BLEND_MODE_ADD:
            case dmParticleDDF::BLEND_MODE_ADD_ALPHA:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmParticleDDF::BLEND_MODE_MULT:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmParticleDDF::BLEND_MODE_SCREEN:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
            break;
        }
    }

    static void SetRenderConstants(dmRender::HNamedConstantBuffer constant_buffer, dmParticle::RenderConstant* constants, uint32_t constant_count)
    {
        for (uint32_t i = 0; i < constant_count; ++i)
        {
            dmParticle::RenderConstant* c = &constants[i];

            dmRenderDDF::MaterialDesc::ConstantType type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;
            if (c->m_IsMatrix4)
            {
                type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4;
            }
            dmRender::SetNamedConstant(constant_buffer, c->m_NameHash, (dmVMath::Vector4*) &c->m_Value, c->m_IsMatrix4 ? 4 : 1, type);
        }
    }

    void RenderLineCallback(void* usercontext, const dmVMath::Point3& start, const dmVMath::Point3& end, const dmVMath::Vector4& color)
    {
        dmRender::Line3D((dmRender::HRenderContext)usercontext, start, end, color, color);
    }

    dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data)
    {
        TextureSetResource* texture_set_res = (TextureSetResource*)texture_set_ptr;
        dmGameSystemDDF::TextureSet* texture_set = texture_set_res->m_TextureSet;
        uint32_t* anim_index = texture_set_res->m_AnimationIds.Get(animation);
        if (anim_index)
        {
            if (texture_set_res->m_TextureSet->m_TexCoords.m_Count == 0)
            {
                return dmParticle::FETCH_ANIMATION_UNKNOWN_ERROR;
            }

            out_data->m_Texture = (void*) texture_set_res->m_Texture;
            out_data->m_TexCoords = (float*) texture_set_res->m_TextureSet->m_TexCoords.m_Data;
            out_data->m_TexDims = (float*) texture_set_res->m_TextureSet->m_TexDims.m_Data;
            out_data->m_PageIndices = texture_set_res->m_TextureSet->m_PageIndices.m_Data;
            out_data->m_FrameIndices = texture_set_res->m_TextureSet->m_FrameIndices.m_Data;
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_Animations[*anim_index];
            out_data->m_FPS = animation->m_Fps;
            out_data->m_TileWidth = animation->m_Width;
            out_data->m_TileHeight = animation->m_Height;
            out_data->m_StartTile = animation->m_Start;
            out_data->m_EndTile = animation->m_End;
            out_data->m_HFlip = animation->m_FlipHorizontal;
            out_data->m_VFlip = animation->m_FlipVertical;
            switch (animation->m_Playback)
            {
            case dmGameSystemDDF::PLAYBACK_NONE:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_NONE;
                break;
            case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_ONCE_FORWARD;
                break;
            case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_ONCE_BACKWARD;
                break;
            case dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_ONCE_PINGPONG;
                break;
            case dmGameSystemDDF::PLAYBACK_LOOP_FORWARD:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_LOOP_FORWARD;
                break;
            case dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_LOOP_BACKWARD;
                break;
            case dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG:
                out_data->m_Playback = dmParticle::ANIM_PLAYBACK_LOOP_PINGPONG;
                break;
            }
            out_data->m_StructSize = sizeof(dmParticle::AnimationData);
            return dmParticle::FETCH_ANIMATION_OK;
        }
        else
        {
            return dmParticle::FETCH_ANIMATION_NOT_FOUND;
        }
    }

    void GetParticleFXWorldRenderBuffers(void* pfx_world, dmRender::HBufferedRenderBuffer* vx_buffer)
    {
        ParticleFXWorld* world = (ParticleFXWorld*) pfx_world;
        *vx_buffer = world->m_VertexBuffer;
    }
}
