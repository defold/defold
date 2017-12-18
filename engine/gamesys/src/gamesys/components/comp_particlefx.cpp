#include "comp_particlefx.h"

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "resources/res_particlefx.h"
#include "resources/res_textureset.h"

namespace dmGameSystem
{
    struct ParticleFXWorld;

    struct ParticleFXComponentPrototype
    {
        Vector3 m_Translation;
        Quat m_Rotation;
        dmParticle::HPrototype m_ParticlePrototype;
        uint16_t m_AddedToUpdate : 1;
        uint16_t m_Padding : 15;
    };

    struct ParticleFXComponent
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HInstance m_ParticleInstance;
        dmParticle::HPrototype m_ParticlePrototype;
        ParticleFXWorld* m_World;
        uint32_t m_PrototypeIndex;
        uint16_t m_AddedToUpdate : 1;
        uint16_t m_Padding : 15;
    };

    struct ParticleFXWorld
    {
        dmArray<ParticleFXComponent> m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmArray<ParticleFXComponentPrototype> m_Prototypes;
        dmIndexPool32 m_PrototypeIndices;
        ParticleFXContext* m_Context;
        dmParticle::HParticleContext m_ParticleContext;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmArray<dmParticle::Vertex> m_VertexBufferData;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        uint32_t m_EmitterCount;
        float m_DT;
        uint32_t m_WarnOutOfROs : 1;
    };

    dmGameObject::CreateResult CompParticleFXNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        assert(params.m_Context);

        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* world = new ParticleFXWorld();
        world->m_Context = ctx;
        uint32_t particle_fx_count = ctx->m_MaxParticleFXCount;
        world->m_ParticleContext = dmParticle::CreateContext(particle_fx_count, ctx->m_MaxParticleCount);
        world->m_Components.SetCapacity(particle_fx_count);
        world->m_RenderObjects.SetCapacity(particle_fx_count);
        world->m_Prototypes.SetCapacity(particle_fx_count);
        world->m_Prototypes.SetSize(particle_fx_count);
        world->m_PrototypeIndices.SetCapacity(particle_fx_count);
        uint32_t buffer_size = dmParticle::GetVertexBufferSize(ctx->m_MaxParticleCount, dmParticle::PARTICLE_GO);
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(ctx->m_RenderContext), buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        world->m_VertexBufferData.SetCapacity(ctx->m_MaxParticleCount * 6);
        world->m_WarnOutOfROs = 0;
        world->m_EmitterCount = 0;
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
            {"color", 1, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
            {"texcoord0", 2, 2, dmGraphics::TYPE_UNSIGNED_SHORT, true},
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(ctx->m_RenderContext), ve, 3);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ParticleFXWorld* pfx_world = (ParticleFXWorld*)params.m_World;
        for (uint32_t i = 0; i < pfx_world->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &pfx_world->m_Components[i];
            dmResource::Release(pfx_world->m_Context->m_Factory, c->m_ParticlePrototype);
            dmParticle::DestroyInstance(pfx_world->m_ParticleContext, c->m_ParticleInstance);
        }
        dmParticle::DestroyContext(pfx_world->m_ParticleContext);
        dmGraphics::DeleteVertexBuffer(pfx_world->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(pfx_world->m_VertexDeclaration);
        delete pfx_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*)params.m_World;
        if (world->m_PrototypeIndices.Remaining() == 0)
        {
            dmLogError("ParticleFX could not be created since the buffer is full (%d).", world->m_PrototypeIndices.Capacity());
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
    static void SetRenderConstants(dmRender::RenderObject* ro, dmParticle::RenderConstant* constants, uint32_t constant_count);
    void RenderLineCallback(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color);
    dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data);

    dmGameObject::CreateResult CompParticleFXAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
        prototype->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        w->m_DT = params.m_UpdateContext->m_DT;
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
                dmParticle::SetScaleAlongZ(particle_context, c.m_ParticleInstance, dmGameObject::ScaleAlongZ(c.m_Instance));
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
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderBatch(ParticleFXWorld* pfx_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        const dmParticle::EmitterRenderData* first = (dmParticle::EmitterRenderData*) buf[*begin].m_UserData;
        ParticleFXContext* pfx_context = pfx_world->m_Context;
        dmParticle::HParticleContext particle_context = pfx_world->m_ParticleContext;

        dmArray<dmParticle::Vertex> &vertex_buffer = pfx_world->m_VertexBufferData;
        dmParticle::Vertex* vb_begin = vertex_buffer.End();
        dmParticle::Vertex* vb_end = vb_begin;

        uint32_t vb_size_init = vertex_buffer.Size() * sizeof(dmParticle::Vertex);
        uint32_t vb_size = vb_size_init;
        uint32_t vb_max_size =  dmParticle::GetVertexBufferSize(pfx_context->m_MaxParticleCount, dmParticle::PARTICLE_GO);

        for (uint32_t *i = begin; i != end; ++i)
        {
            const dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*) buf[*i].m_UserData;
            dmParticle::GenerateVertexData(particle_context, pfx_world->m_DT, emitter_render_data->m_Instance, emitter_render_data->m_EmitterIndex, Vector4(1,1,1,1), (void*)vertex_buffer.Begin(), vb_max_size, &vb_size, dmParticle::PARTICLE_GO);
        }

        vb_end = (vb_begin + (vb_size - vb_size_init) / sizeof(dmParticle::Vertex));

        uint32_t ro_vertex_count = vb_end - vb_begin;
        vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());

        // Ninja in-place writing of render object
        dmRender::RenderObject& ro = *pfx_world->m_RenderObjects.End();
        pfx_world->m_RenderObjects.SetSize(pfx_world->m_RenderObjects.Size()+1);
        ro.Init();
        ro.m_Material = (dmRender::HMaterial)first->m_Material;
        ro.m_Textures[0] = (dmGraphics::HTexture)first->m_Texture;
        ro.m_VertexStart = vb_begin - vertex_buffer.Begin();
        ro.m_VertexCount = ro_vertex_count;
        ro.m_VertexBuffer = pfx_world->m_VertexBuffer;
        ro.m_VertexDeclaration = pfx_world->m_VertexDeclaration;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_WorldTransform = first->m_Transform;
        ro.m_SetBlendFactors = 1;
        SetBlendFactors(&ro, first->m_BlendMode);
        SetRenderConstants(&ro, first->m_RenderConstants, first->m_RenderConstantsSize);

        dmRender::AddToRender(render_context, &ro);
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        ParticleFXWorld* pfx_world = (ParticleFXWorld*)params.m_UserData;

        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BEGIN)
        {
            dmGraphics::SetVertexBufferData(pfx_world->m_VertexBuffer, 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
            pfx_world->m_VertexBufferData.SetSize(0);
            pfx_world->m_RenderObjects.SetSize(0);
        }
        else if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            RenderBatch(pfx_world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
        }
        else if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_END)
        {
            dmGraphics::SetVertexBufferData(pfx_world->m_VertexBuffer, sizeof(dmParticle::Vertex) * pfx_world->m_VertexBufferData.Size(), 
                                            pfx_world->m_VertexBufferData.Begin(), dmGraphics::BUFFER_USAGE_STREAM_DRAW);
            DM_COUNTER("ParticleFXVertexBuffer", pfx_world->m_VertexBufferData.Size() * sizeof(dmParticle::Vertex));
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
                uint32_t emitter_count = dmParticle::GetEmitterCount(c.m_ParticlePrototype);
                for (int j = 0; j < emitter_count; ++j)
                {
                    dmParticle::EmitterRenderData* render_data;
                    dmParticle::GetEmitterRenderData(particle_context, c.m_ParticleInstance, j, &render_data);

                    write_ptr->m_WorldPosition = Point3(render_data->m_Transform.getTranslation());
                    write_ptr->m_UserData = (uintptr_t) render_data;
                    write_ptr->m_BatchKey = render_data->m_MixedHash;
                    write_ptr->m_TagMask = dmRender::GetMaterialTagMask((dmRender::HMaterial)render_data->m_Material);
                    write_ptr->m_Dispatch = dispatch;
                    write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
                    ++write_ptr;
                }
            }
        }

        dmRender::RenderListSubmit(ctx->m_RenderContext, render_list, write_ptr);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmParticle::HInstance CreateComponent(ParticleFXWorld* world, dmGameObject::HInstance go_instance, ParticleFXComponentPrototype* prototype, dmParticle::EmitterStateChangedData* emitter_state_changed_data)
    {
        if (!world->m_Components.Full())
        {
            uint32_t count = world->m_Components.Size();
            world->m_Components.SetSize(count + 1);
            ParticleFXComponent* component = &world->m_Components[count];
            component->m_Instance = go_instance;
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
        else
        {
            dmLogError("Particle FX component buffer is full (%d), component disregarded.", world->m_Components.Capacity());
            return dmParticle::INVALID_INSTANCE;
        }
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

            dmParticle::HInstance instance = CreateComponent(world, params.m_Instance, prototype, &emitter_state_changed_data);

            if (prototype->m_AddedToUpdate)
            {
                dmParticle::StartInstance(particle_context, instance);
            }

            dmTransform::Transform world_transform(prototype->m_Translation, prototype->m_Rotation, 1.0f);
            world_transform = dmTransform::Mul(dmGameObject::GetWorldTransform(params.m_Instance), world_transform);
            dmParticle::SetPosition(particle_context, instance, Point3(world_transform.GetTranslation()));
            dmParticle::SetRotation(particle_context, instance, world_transform.GetRotation());
            dmParticle::SetScale(particle_context, instance, world_transform.GetUniformScale());
            dmParticle::SetScaleAlongZ(particle_context, instance, dmGameObject::ScaleAlongZ(params.m_Instance));
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash)
        {
            uint32_t count = world->m_Components.Size();
            for (uint32_t i = 0; i < count; ++i)
            {
                ParticleFXComponent* component = &world->m_Components[i];
                if (component->m_Instance == params.m_Instance)
                {
                    dmParticle::StopInstance(world->m_ParticleContext, component->m_ParticleInstance);
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
                    dmParticle::SetRenderConstant(world->m_ParticleContext, component->m_ParticleInstance, ddf->m_EmitterId, ddf->m_NameHash, ddf->m_Value);
                    ++found_count;
                }
            }
            if (found_count == 0)
            {
                dmLogWarning("Particle FX to set constant for could not be found.");
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

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
            break;
        }
    }

    static void SetRenderConstants(dmRender::RenderObject* ro, dmParticle::RenderConstant* constants, uint32_t constant_count)
    {
        for (uint32_t i = 0; i < constant_count; ++i)
        {
            dmParticle::RenderConstant* c = &constants[i];
            dmRender::EnableRenderObjectConstant(ro, c->m_NameHash, c->m_Value);
        }
    }

    void RenderLineCallback(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color)
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
            out_data->m_Texture = texture_set_res->m_Texture;
            out_data->m_TexCoords = (float*) texture_set_res->m_TextureSet->m_TexCoords.m_Data;
            out_data->m_TexDims = (float*) texture_set_res->m_TextureSet->m_TexDims.m_Data;
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
}
