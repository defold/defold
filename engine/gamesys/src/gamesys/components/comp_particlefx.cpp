#include "comp_particlefx.h"

#include <float.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "gamesys.h"
#include "../gamesys_private.h"

namespace dmGameSystem
{
    const uint32_t MAX_COMPONENT_COUNT = 64;

    struct ParticleFXWorld;

    struct ParticleFXComponent
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HInstance m_ParticleFXInstance;
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

    struct ParticleVertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Alpha;
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
        uint32_t buffer_size = ctx->m_MaxParticleCount * 6 * sizeof(ParticleVertex);
        world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(ctx->m_RenderContext), buffer_size, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        world->m_ClientBuffer = new char[buffer_size];
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 3, dmGraphics::TYPE_FLOAT},
            {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT},
            {"alpha", 2, 1, dmGraphics::TYPE_FLOAT}
        };
        world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(ctx->m_RenderContext), ve, 3);
        *params.m_World = world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ParticleFXWorld* emitter_world = (ParticleFXWorld*)params.m_World;
        dmParticle::DestroyContext(emitter_world->m_ParticleContext);
        delete [] (char*)emitter_world->m_ClientBuffer;
        dmGraphics::DeleteVertexBuffer(emitter_world->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(emitter_world->m_VertexDeclaration);
        delete emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompParticleFXCreate(const dmGameObject::ComponentCreateParams& params)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)params.m_Resource;
        assert(prototype);
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        if (w->m_Components.Size() < MAX_COMPONENT_COUNT)
        {
            ParticleFXComponent emitter;
            emitter.m_Instance = params.m_Instance;
            emitter.m_ParticleFXInstance = dmParticle::CreateInstance(w->m_ParticleContext, prototype);
            emitter.m_World = w;
            w->m_Components.Push(emitter);
            *params.m_UserData = (uintptr_t)&w->m_Components[w->m_Components.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Particle component buffer is full (%d), component disregarded.", MAX_COMPONENT_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompParticleFXDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        for (uint32_t i = 0; i < w->m_Components.Size(); ++i)
        {
            if (w->m_Components[i].m_Instance == params.m_Instance)
            {
                dmParticle::DestroyInstance(w->m_ParticleContext, w->m_Components[i].m_ParticleFXInstance);
                w->m_Components.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }

        dmLogError("Destroyed emitter could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    void RenderInstanceCallback(void* render_context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count);
    void RenderLineCallback(void* render_context, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color);

    dmGameObject::UpdateResult CompParticleFXUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        ParticleFXWorld* w = (ParticleFXWorld*)params.m_World;
        if (w->m_Components.Size() == 0)
            return dmGameObject::UPDATE_RESULT_OK;

        dmParticle::HContext particle_context = w->m_ParticleContext;
        for (uint32_t i = 0; i < w->m_Components.Size(); ++i)
        {
            ParticleFXComponent& emitter = w->m_Components[i];
            Point3 position = dmGameObject::GetWorldPosition(emitter.m_Instance);
            dmParticle::SetPosition(particle_context, emitter.m_ParticleFXInstance, position);
            dmParticle::SetRotation(particle_context, emitter.m_ParticleFXInstance, dmGameObject::GetWorldRotation(emitter.m_Instance));
        }
        ParticleFXContext* ctx = (ParticleFXContext*)params.m_Context;

        // NOTE: Objects are added in RenderEmitterCallback
        w->m_RenderObjects.SetSize(0);

        float* vertex_buffer = (float*)w->m_ClientBuffer;
        uint32_t max_vertex_buffer_size = ctx->m_MaxParticleCount * 6 * sizeof(ParticleVertex);
        uint32_t vertex_buffer_size;
        dmParticle::Update(particle_context, params.m_UpdateContext->m_DT, vertex_buffer, max_vertex_buffer_size, &vertex_buffer_size);
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
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompParticleFXOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ParticleFXComponent* emitter = (ParticleFXComponent*)*params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64("start"))
        {
            dmParticle::StartInstance(emitter->m_World->m_ParticleContext, emitter->m_ParticleFXInstance);
        }
        else if (params.m_Message->m_Id == dmHashString64("restart"))
        {
            dmParticle::RestartInstance(emitter->m_World->m_ParticleContext, emitter->m_ParticleFXInstance);
        }
        else if (params.m_Message->m_Id == dmHashString64("stop"))
        {
            dmParticle::StopInstance(emitter->m_World->m_ParticleContext, emitter->m_ParticleFXInstance);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompParticleFXOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        ParticleFXComponent* component = (ParticleFXComponent*)*params.m_UserData;
        dmParticle::RestartInstance(component->m_World->m_ParticleContext, component->m_ParticleFXInstance);
    }

    void RenderInstanceCallback(void* context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
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
        world->m_RenderObjects.Push(ro);
    }

    void RenderLineCallback(void* usercontext, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color)
    {
        dmRender::Line3D((dmRender::HRenderContext)usercontext, start, end, color, color);
    }
}
