#include "comp_particlefx.h"

#include <float.h>
#include <stdio.h>
#include <assert.h>

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
#include "resources/res_tileset.h"

namespace dmGameSystem
{
    struct ParticleFXWorld;

    struct ParticleFXComponent
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HInstance m_ParticleFXInstance;
        dmParticle::HPrototype m_ParticleFXPrototype;
        ParticleFXWorld* m_World;
    };

    struct ParticleFXWorld
    {
        dmArray<ParticleFXComponent> m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        ParticleFXContext* m_Context;
        dmParticle::HContext m_ParticleContext;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        void* m_ClientBuffer;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        uint32_t m_VertexCount;
    };

    dmGameObject::CreateResult CompParticleFXNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        assert(params.m_Context);

        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;
        ParticleFXWorld* world = new ParticleFXWorld();
        world->m_Context = ctx;
        world->m_ParticleContext = dmParticle::CreateContext(ctx->m_MaxParticleFXCount, ctx->m_MaxParticleCount);
        world->m_Components.SetCapacity(ctx->m_MaxParticleFXCount);
        world->m_RenderObjects.SetCapacity(ctx->m_MaxParticleFXCount);
        uint32_t buffer_size = dmParticle::GetVertexBufferSize(ctx->m_MaxParticleCount);
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(ctx->m_RenderContext), buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        world->m_ClientBuffer = new char[buffer_size];
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
            {"color", 1, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
            {"texcoord0", 2, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(ctx->m_RenderContext), ve, 3);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ParticleFXWorld* emitter_world = (ParticleFXWorld*)params.m_World;
        for (uint32_t i = 0; i < emitter_world->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &emitter_world->m_Components[i];
            dmResource::Release(emitter_world->m_Context->m_Factory, c->m_ParticleFXPrototype);
            dmParticle::DestroyInstance(emitter_world->m_ParticleContext, c->m_ParticleFXInstance);
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
        *params.m_UserData = (uintptr_t)params.m_Resource;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        for (uint32_t i = 0; i < w->m_Components.Size(); ++i)
        {
            ParticleFXComponent* c = &w->m_Components[i];
            if (c->m_Instance == params.m_Instance)
            {
                c->m_Instance = 0;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    void RenderInstanceCallback(void* render_context, void* material, void* texture, dmParticleDDF::BlendMode blend_mode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count);
    void RenderLineCallback(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color);
    dmParticle::FetchAnimationResult FetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data);

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
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
                dmParticle::SetPosition(particle_context, c.m_ParticleFXInstance, dmGameObject::GetWorldPosition(c.m_Instance));
                dmParticle::SetRotation(particle_context, c.m_ParticleFXInstance, dmGameObject::GetWorldRotation(c.m_Instance));
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
        uint32_t n = w->m_RenderObjects.Size();
        dmArray<dmRender::RenderObject>& render_objects = w->m_RenderObjects;
        for (uint32_t i = 0; i < n; ++i)
        {
            dmRender::AddToRender(render_context, &render_objects[i]);
        }

        if (ctx->m_Debug)
        {
            dmParticle::DebugRender(particle_context, render_context, RenderLineCallback);
        }

        // Prune sleeping instances
        uint32_t i = 0;
        while (i < count)
        {
            ParticleFXComponent& c = components[i];
            if (dmParticle::IsSleeping(particle_context, c.m_ParticleFXInstance))
            {
                dmResource::Release(ctx->m_Factory, c.m_ParticleFXPrototype);
                dmParticle::DestroyInstance(particle_context, c.m_ParticleFXInstance);
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

    static dmParticle::HInstance CreateComponent(ParticleFXWorld* world, dmGameObject::HInstance go_instance, dmParticle::HPrototype prototype)
    {
        if (!world->m_Components.Full())
        {
            uint32_t count = world->m_Components.Size();
            world->m_Components.SetSize(count + 1);
            ParticleFXComponent* emitter = &world->m_Components[count];
            emitter->m_Instance = go_instance;
            // NOTE: We must increase ref-count as a particle fx might be playing after the component is destroyed
            dmResource::HFactory factory = world->m_Context->m_Factory;
            dmResource::IncRef(factory, prototype);
            emitter->m_ParticleFXInstance = dmParticle::CreateInstance(world->m_ParticleContext, prototype);
            emitter->m_ParticleFXPrototype = prototype;
            emitter->m_World = world;
            return emitter->m_ParticleFXInstance;
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
            dmParticle::HContext context = world->m_ParticleContext;
            dmParticle::HInstance instance = CreateComponent(world, params.m_Instance, (dmParticle::HPrototype)*params.m_UserData);
            dmParticle::StartInstance(context, instance);
            dmParticle::SetPosition(context, instance, dmGameObject::GetWorldPosition(params.m_Instance));
            dmParticle::SetRotation(context, instance, dmGameObject::GetWorldRotation(params.m_Instance));
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
                    dmParticle::StopInstance(world->m_ParticleContext, component->m_ParticleFXInstance);
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
                    dmParticle::SetRenderConstant(world->m_ParticleContext, component->m_ParticleFXInstance, ddf->m_EmitterId, ddf->m_NameHash, ddf->m_Value);
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
                    dmParticle::ResetRenderConstant(world->m_ParticleContext, component->m_ParticleFXInstance, ddf->m_EmitterId, ddf->m_NameHash);
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
        uint32_t count = world->m_Components.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ParticleFXComponent* component = &world->m_Components[i];
            if (component->m_ParticleFXPrototype == params.m_Resource)
            {
                dmParticle::ReloadInstance(world->m_ParticleContext, component->m_ParticleFXInstance, true);
            }
        }
        // Don't warn if none could be found
    }

    static void SetBlendFactors(dmRender::RenderObject* ro, dmParticleDDF::BlendMode blend_mode)
    {
        switch (blend_mode)
        {
            case dmParticleDDF::BLEND_MODE_ALPHA:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmParticleDDF::BLEND_MODE_ADD:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmParticleDDF::BLEND_MODE_ADD_ALPHA:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmParticleDDF::BLEND_MODE_MULT:
                ro->m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ZERO;
                ro->m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_SRC_COLOR;
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

    void RenderInstanceCallback(void* context, void* material, void* texture, dmParticleDDF::BlendMode blend_mode, uint32_t vertex_index, uint32_t vertex_count, dmParticle::RenderConstant* constants, uint32_t constant_count)
    {
        ParticleFXWorld* world = (ParticleFXWorld*)context;
        dmRender::RenderObject ro;
        ro.m_Material = (dmRender::HMaterial)material;
        ro.m_Textures[0] = (dmGraphics::HTexture)texture;
        ro.m_VertexStart = vertex_index;
        ro.m_VertexCount = vertex_count;
        ro.m_VertexBuffer = world->m_VertexBuffer;
        ro.m_VertexDeclaration = world->m_VertexDeclaration;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_CalculateDepthKey = 1;
        ro.m_SetBlendFactors = 1;
        SetBlendFactors(&ro, blend_mode);
        SetRenderConstants(&ro, constants, constant_count);
        world->m_RenderObjects.Push(ro);
    }

    void RenderLineCallback(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color)
    {
        dmRender::Line3D((dmRender::HRenderContext)usercontext, start, end, color, color);
    }

    dmParticle::FetchAnimationResult FetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data)
    {
        TileSetResource* tile_set_res = (TileSetResource*)tile_source;
        dmGameSystemDDF::TileSet* tile_set = tile_set_res->m_TileSet;
        uint32_t anim_count = tile_set_res->m_AnimationIds.Size();
        uint32_t anim_index = ~0u;
        for (uint32_t i = 0; i < anim_count; ++i)
        {
            if (tile_set_res->m_AnimationIds[i] == animation)
            {
                anim_index = i;
                break;
            }
        }
        if (anim_index != ~0u)
        {
            if (tile_set_res->m_TexCoords.Size() == 0)
            {
                return dmParticle::FETCH_ANIMATION_UNKNOWN_ERROR;
            }
            out_data->m_Texture = tile_set_res->m_Texture;
            out_data->m_TexCoords = &tile_set_res->m_TexCoords.Front();
            dmGameSystemDDF::Animation* animation = &tile_set->m_Animations[anim_index];
            out_data->m_FPS = animation->m_Fps;
            out_data->m_TileWidth = tile_set->m_TileWidth;
            out_data->m_TileHeight = tile_set->m_TileHeight;
            out_data->m_StartTile = animation->m_StartTile;
            out_data->m_EndTile = animation->m_EndTile;
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
