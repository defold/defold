#include "comp_emitter.h"

#include <float.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "gamesys.h"

namespace dmGameSystem
{
    const uint32_t MAX_EMITTER_COUNT = 64;

    struct EmitterWorld;

    struct Emitter
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HEmitter m_Emitter;
        EmitterWorld* m_World;
    };

    struct EmitterWorld
    {
        dmArray<Emitter> m_Emitters;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        EmitterContext* m_EmitterContext;
        dmParticle::HContext m_ParticleContext;
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        uint32_t m_VertexCount;
    };

    struct ParticleVertex
    {
        float m_Position[3];
        float m_UV[2];
        float m_Alpha;
    };

    dmGameObject::CreateResult CompEmitterNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        assert(params.m_Context);
        EmitterContext* ctx = (EmitterContext*)params.m_Context;
        EmitterWorld* emitter_world = new EmitterWorld();
        emitter_world->m_EmitterContext = ctx;
        emitter_world->m_ParticleContext = dmParticle::CreateContext(ctx->m_MaxEmitterCount, ctx->m_MaxParticleCount);
        emitter_world->m_Emitters.SetCapacity(MAX_EMITTER_COUNT);
        emitter_world->m_RenderObjects.SetCapacity(MAX_EMITTER_COUNT);
        emitter_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(ctx->m_RenderContext), ctx->m_MaxParticleCount * 6 * sizeof(ParticleVertex), 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::VertexElement ve[] =
        {
            {"position", 0, 3, dmGraphics::TYPE_FLOAT},
            {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT},
            {"alpha", 2, 1, dmGraphics::TYPE_FLOAT}
        };
        emitter_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(ctx->m_RenderContext), ve, 3);
        *params.m_World = emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        EmitterWorld* emitter_world = (EmitterWorld*)params.m_World;
        dmParticle::DestroyContext(emitter_world->m_ParticleContext);
        dmGraphics::DeleteVertexBuffer(emitter_world->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(emitter_world->m_VertexDeclaration);
        delete emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterCreate(const dmGameObject::ComponentCreateParams& params)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)params.m_Resource;
        assert(prototype);
        EmitterWorld* w = (EmitterWorld*)params.m_World;
        if (w->m_Emitters.Size() < MAX_EMITTER_COUNT)
        {
            Emitter emitter;
            emitter.m_Instance = params.m_Instance;
            emitter.m_Emitter = dmParticle::CreateEmitter(w->m_ParticleContext, prototype);
            emitter.m_World = w;
            w->m_Emitters.Push(emitter);
            *params.m_UserData = (uintptr_t)&w->m_Emitters[w->m_Emitters.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Emitter buffer is full (%d), component disregarded.", MAX_EMITTER_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompEmitterDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        EmitterWorld* w = (EmitterWorld*)params.m_World;
        for (uint32_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            if (w->m_Emitters[i].m_Instance == params.m_Instance)
            {
                dmParticle::DestroyEmitter(w->m_ParticleContext, w->m_Emitters[i].m_Emitter);
                w->m_Emitters.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }

        dmLogError("Destroyed emitter could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    void RenderEmitterCallback(void* render_context, const Point3& position, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count);
    void RenderLineCallback(void* render_context, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color);

    dmGameObject::UpdateResult CompEmitterUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {

        EmitterWorld* w = (EmitterWorld*)params.m_World;
        if (w->m_Emitters.Size() == 0)
            return dmGameObject::UPDATE_RESULT_OK;

        for (uint32_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            Emitter& emitter = w->m_Emitters[i];
            Point3 position = dmGameObject::GetWorldPosition(emitter.m_Instance);
            dmParticle::SetPosition(w->m_ParticleContext, emitter.m_Emitter, position);
            dmParticle::SetRotation(w->m_ParticleContext, emitter.m_Emitter, dmGameObject::GetWorldRotation(emitter.m_Instance));
        }
        EmitterContext* ctx = (EmitterContext*)params.m_Context;

        // NOTE: Objects are added in RenderEmitterCallback
        w->m_RenderObjects.SetSize(0);

        float* vertex_buffer = (float*)dmGraphics::MapVertexBuffer(w->m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        uint32_t vertex_buffer_size = ctx->m_MaxParticleCount * 6 * sizeof(ParticleVertex);
        dmParticle::Update(w->m_ParticleContext, params.m_UpdateContext->m_DT, vertex_buffer, vertex_buffer_size);
        dmGraphics::UnmapVertexBuffer(w->m_VertexBuffer);
        dmParticle::Render(w->m_ParticleContext, w, RenderEmitterCallback);
        uint32_t n = w->m_RenderObjects.Size();
        dmArray<dmRender::RenderObject>& render_objects = w->m_RenderObjects;

        for (uint32_t i = 0; i < n; ++i)
        {
            dmRender::AddToRender(ctx->m_RenderContext, &render_objects[i]);
        }

        if (ctx->m_Debug)
        {
            EmitterContext* emitter_context = (EmitterContext*)params.m_Context;
            dmParticle::DebugRender(w->m_ParticleContext, emitter_context->m_RenderContext, RenderLineCallback);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompEmitterOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        Emitter* emitter = (Emitter*)*params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64("start"))
        {
            dmParticle::StartEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        else if (params.m_Message->m_Id == dmHashString64("restart"))
        {
            dmParticle::RestartEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        else if (params.m_Message->m_Id == dmHashString64("stop"))
        {
            dmParticle::StopEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompEmitterOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        Emitter* emitter = (Emitter*)*params.m_UserData;
        dmParticle::RestartEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
    }

    void RenderEmitterCallback(void* context, const Point3& position, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
    {
        EmitterWorld* world = (EmitterWorld*)context;
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
