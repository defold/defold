#include "comp_particlefx.h"

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
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
        dmParticle::HContext m_ParticleContext;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        void* m_ClientBuffer;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        uint32_t m_VertexCount;
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
        uint32_t buffer_size = dmParticle::GetVertexBufferSize(ctx->m_MaxParticleCount);
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(ctx->m_RenderContext), buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        world->m_ClientBuffer = new char[buffer_size];
        world->m_WarnOutOfROs = 0;
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
            {"color", 1, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
            {"texcoord0", 2, 2, dmGraphics::TYPE_UNSIGNED_SHORT, true},
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(ctx->m_RenderContext), ve, 3);
        params.m_World->m_Ptr = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ParticleFXWorld* emitter_world = (ParticleFXWorld*) params.m_World.m_Ptr;
        for (uint32_t i = 0; i < emitter_world->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &emitter_world->m_Components[i];
            dmResource::Release(emitter_world->m_Context->m_Factory, c->m_ParticlePrototype);
            dmParticle::DestroyInstance(emitter_world->m_ParticleContext, c->m_ParticleInstance);
        }
        dmParticle::DestroyContext(emitter_world->m_ParticleContext);
        delete [] (char*)emitter_world->m_ClientBuffer;
        dmGraphics::DeleteVertexBuffer(emitter_world->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(emitter_world->m_VertexDeclaration);
        delete emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*) params.m_World.m_Ptr;
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
        ParticleFXWorld* w = (ParticleFXWorld*) params.m_World.m_Ptr;
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

    void RenderInstanceCallback(void* render_context, void* material, void* texture, const Vectormath::Aos::Matrix4& world_transform, dmParticleDDF::BlendMode blend_mode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count);
    void RenderLineCallback(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color);
    dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data);

    dmGameObject::CreateResult CompParticleFXAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
        prototype->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*) params.m_World.m_Ptr;
        dmArray<ParticleFXComponent>& components = w->m_Components;
        if (components.Empty())
            return dmGameObject::UPDATE_RESULT_OK;

        dmParticle::HContext particle_context = w->m_ParticleContext;
        uint32_t count = components.Size();

        // Update positions
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

        // NOTE: Objects are added in RenderEmitterCallback
        w->m_RenderObjects.SetSize(0);

        uint32_t max_vertex_buffer_size =  dmParticle::GetVertexBufferSize(ctx->m_MaxParticleCount);
        uint32_t vertex_buffer_size;
        dmParticle::Update(particle_context, params.m_UpdateContext->m_DT, w->m_ClientBuffer, max_vertex_buffer_size, &vertex_buffer_size, FetchAnimationCallback);
        dmParticle::Render(particle_context, w, RenderInstanceCallback);
        dmGraphics::SetVertexBufferData(w->m_VertexBuffer, 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::SetVertexBufferData(w->m_VertexBuffer, vertex_buffer_size, w->m_ClientBuffer, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmRender::HRenderContext render_context = ctx->m_RenderContext;

        if (ctx->m_Debug)
        {
            dmParticle::DebugRender(particle_context, render_context, RenderLineCallback);
        }

        // Prune sleeping instances
        uint32_t i = 0;
        while (i < count)
        {
            ParticleFXComponent& c = components[i];
            if ((c.m_AddedToUpdate || c.m_Instance == 0) && dmParticle::IsSleeping(particle_context, c.m_ParticleInstance))
            {
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

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            for (uint32_t *i=params.m_Begin;i!=params.m_End;i++)
            {
                dmRender::RenderObject *ro = (dmRender::RenderObject*) params.m_Buf[*i].m_UserData;
                dmRender::AddToRender(params.m_Context, ro);
            }
        }
    }

    dmGameObject::UpdateResult CompParticleFXRender(const dmGameObject::ComponentsRenderParams& params)
    {
        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* w = (ParticleFXWorld*) params.m_World.m_Ptr;

        dmArray<dmRender::RenderObject>& render_objects = w->m_RenderObjects;
        const uint32_t n = w->m_RenderObjects.Size();

        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(ctx->m_RenderContext, n);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(ctx->m_RenderContext, &RenderListDispatch, 0);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < n; ++i)
        {
            const Vector4 trans = render_objects[i].m_WorldTransform.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &render_objects[i];
            write_ptr->m_BatchKey = 0;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(render_objects[i].m_Material);
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(ctx->m_RenderContext, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }


    static dmParticle::HInstance CreateComponent(ParticleFXWorld* world, dmGameObject::HInstance go_instance, ParticleFXComponentPrototype* prototype)
    {
        if (!world->m_Components.Full())
        {
            uint32_t count = world->m_Components.Size();
            world->m_Components.SetSize(count + 1);
            ParticleFXComponent* emitter = &world->m_Components[count];
            emitter->m_Instance = go_instance;
            emitter->m_PrototypeIndex = prototype - world->m_Prototypes.Begin();
            // NOTE: We must increase ref-count as a particle fx might be playing after the component is destroyed
            dmResource::HFactory factory = world->m_Context->m_Factory;
            dmResource::IncRef(factory, prototype->m_ParticlePrototype);
            emitter->m_ParticleInstance = dmParticle::CreateInstance(world->m_ParticleContext, prototype->m_ParticlePrototype);
            emitter->m_ParticlePrototype = prototype->m_ParticlePrototype;
            emitter->m_World = world;
            emitter->m_AddedToUpdate = prototype->m_AddedToUpdate;
            return emitter->m_ParticleInstance;
        }
        else
        {
            dmLogError("Particle FX component buffer is full (%d), component disregarded.", world->m_Components.Capacity());
            return dmParticle::INVALID_INSTANCE;
        }
    }

    dmGameObject::UpdateResult CompParticleFXOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ParticleFXWorld* world = (ParticleFXWorld*) params.m_World.m_Ptr;
        if (params.m_Message->m_Id == dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash)
        {
            dmParticle::HContext context = world->m_ParticleContext;
            ParticleFXComponentPrototype* prototype = (ParticleFXComponentPrototype*)*params.m_UserData;
            dmParticle::HInstance instance = CreateComponent(world, params.m_Instance, prototype);
            if (prototype->m_AddedToUpdate) {
                dmParticle::StartInstance(context, instance);
            }
            dmTransform::Transform world_transform(prototype->m_Translation, prototype->m_Rotation, 1.0f);
            world_transform = dmTransform::Mul(dmGameObject::GetWorldTransform(params.m_Instance), world_transform);
            dmParticle::SetPosition(context, instance, Point3(world_transform.GetTranslation()));
            dmParticle::SetRotation(context, instance, world_transform.GetRotation());
            dmParticle::SetScale(context, instance, world_transform.GetUniformScale());
            dmParticle::SetScaleAlongZ(context, instance, dmGameObject::ScaleAlongZ(params.m_Instance));
        }
        else if (params.m_Message->m_Id == dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash)
        {
            uint32_t count = world->m_Components.Size();
            uint32_t found_count = 0;
            for (uint32_t i = 0; i < count; ++i)
            {
                ParticleFXComponent* component = &world->m_Components[i];
                if (component->m_Instance == params.m_Instance)
                {
                    dmParticle::StopInstance(world->m_ParticleContext, component->m_ParticleInstance);
                    ++found_count;
                }
            }
            if (found_count == 0)
            {
                dmLogWarning("Particle FX to stop could not be found.");
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
        ParticleFXWorld* world = (ParticleFXWorld*) params.m_World.m_Ptr;
        world->m_WarnOutOfROs = 0;
        uint32_t count = world->m_Components.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent* component = &world->m_Components[i];
            if (component->m_ParticlePrototype == params.m_Resource)
            {
                dmParticle::ReloadInstance(world->m_ParticleContext, component->m_ParticleInstance, true);
            }
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

    void RenderInstanceCallback(void* context, void* material, void* texture, const Vectormath::Aos::Matrix4& world_transform, dmParticleDDF::BlendMode blend_mode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count)
    {
        // TODO currently one render object per emitter, should be batched better
        // https://defold.fogbugz.com/default.asp?1815
        ParticleFXWorld* world = (ParticleFXWorld*)context;
        if (!world->m_RenderObjects.Full())
        {
            dmRender::RenderObject ro;
            ro.m_Material = (dmRender::HMaterial)material;
            ro.m_Textures[0] = (dmGraphics::HTexture)texture;
            ro.m_VertexStart = vertex_index;
            ro.m_VertexCount = vertex_count;
            ro.m_VertexBuffer = world->m_VertexBuffer;
            ro.m_VertexDeclaration = world->m_VertexDeclaration;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_WorldTransform = world_transform;
            ro.m_SetBlendFactors = 1;
            SetBlendFactors(&ro, blend_mode);
            SetRenderConstants(&ro, constants, constant_count);
            world->m_RenderObjects.Push(ro);
        }
        else if (!world->m_WarnOutOfROs)
        {
            world->m_WarnOutOfROs = 1;
            dmLogWarning("Particles could not be rendered since the buffer is full (%d). Tweak \"%s\" in the config file.", world->m_RenderObjects.Capacity(), dmParticle::MAX_INSTANCE_COUNT_KEY);
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
