#include "comp_emitter.h"

#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include <particle/particle.h>

#include <graphics/graphics_device.h>
#include <render/material.h>

#include "gamesys.h"

namespace dmGameSystem
{
    const uint32_t MAX_COUNT = 64;

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
        Vector4 m_Position;
    };

    dmGameObject::CreateResult CompEmitterNewWorld(void* context, void** world)
    {
        assert(context);
        EmitterContext* ctx = (EmitterContext*)context;
        EmitterWorld* emitter_world = new EmitterWorld();
        emitter_world->m_EmitterContext = ctx;
        emitter_world->m_ParticleContext = dmParticle::CreateContext(ctx->m_MaxEmitterCount, ctx->m_MaxParticleCount);
        emitter_world->m_Emitters.SetCapacity(MAX_COUNT);
        emitter_world->m_RenderObjects.SetCapacity(MAX_COUNT);
        emitter_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(ctx->m_MaxParticleCount * sizeof(ParticleVertex), 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        dmGraphics::VertexElement ve[] =
        {
            {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
            {1, 2, dmGraphics::TYPE_FLOAT, 0, 0},
            {2, 1, dmGraphics::TYPE_FLOAT, 0, 0}
        };
        emitter_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(ve, 3);
        *world = emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterDeleteWorld(void* context, void* world)
    {
        EmitterWorld* emitter_world = (EmitterWorld*)world;
        dmParticle::DestroyContext(emitter_world->m_ParticleContext);
        dmGraphics::DeleteVertexBuffer(emitter_world->m_VertexBuffer);
        dmGraphics::DeleteVertexDeclaration(emitter_world->m_VertexDeclaration);
        delete emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterCreate(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        dmParticle::Prototype* prototype = (dmParticle::Prototype*)resource;
        assert(prototype);
        EmitterWorld* w = (EmitterWorld*)world;
        if (w->m_Emitters.Size() < MAX_COUNT)
        {
            Emitter emitter;
            emitter.m_Instance = instance;
            emitter.m_Emitter = dmParticle::CreateEmitter(w->m_ParticleContext, prototype);
            emitter.m_World = w;
            w->m_Emitters.Push(emitter);
            *user_data = (uintptr_t)&w->m_Emitters[w->m_Emitters.Size() - 1];
            return dmGameObject::CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("Emitter buffer is full (%d), component disregarded.", MAX_COUNT);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    dmGameObject::CreateResult CompEmitterDestroy(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        EmitterWorld* w = (EmitterWorld*)world;
        for (uint32_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            if (w->m_Emitters[i].m_Instance == instance)
            {
                dmParticle::DestroyEmitter(w->m_ParticleContext, w->m_Emitters[i].m_Emitter);
                w->m_Emitters.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }

        dmLogError("Destroyed emitter could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    struct SParticleRenderData
    {
        dmRender::HMaterial   m_Material;
        dmGraphics::HTexture    m_Texture;
        float*                  m_VertexData;
        uint32_t                m_VertexStride;
        uint32_t                m_VertexIndex;
        uint32_t                m_VertexCount;
    };

    void RenderEmitterCallback(void* render_context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count);
    void RenderLineCallback(void* render_context, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color);

    dmGameObject::UpdateResult CompEmitterUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context)
    {

        EmitterWorld* w = (EmitterWorld*)world;

        for (uint32_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            Emitter& emitter = w->m_Emitters[i];
            dmParticle::SetPosition(w->m_ParticleContext, emitter.m_Emitter, dmGameObject::GetWorldPosition(emitter.m_Instance));
            dmParticle::SetRotation(w->m_ParticleContext, emitter.m_Emitter, dmGameObject::GetWorldRotation(emitter.m_Instance));
        }
        EmitterContext* ctx = (EmitterContext*)context;

        w->m_RenderObjects.SetSize(0);

        float* vertex_buffer = (float*)dmGraphics::MapVertexBuffer(w->m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        uint32_t vertex_buffer_size = MAX_COUNT * ctx->m_MaxParticleCount * sizeof(ParticleVertex);
        dmParticle::Update(w->m_ParticleContext, update_context->m_DT, vertex_buffer, vertex_buffer_size);
        dmGraphics::UnmapVertexBuffer(w->m_VertexBuffer);
        dmParticle::Render(w->m_ParticleContext, w, RenderEmitterCallback);
        for (uint32_t i = 0; i < w->m_RenderObjects.Size(); ++i)
        {
            dmRender::AddToRender(ctx->m_RenderContext, &w->m_RenderObjects[i]);
        }

        if (ctx->m_Debug)
        {
            EmitterContext* emitter_context = (EmitterContext*)context;
            dmParticle::DebugRender(w->m_ParticleContext, emitter_context->m_RenderContext, RenderLineCallback);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompEmitterOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        Emitter* emitter = (Emitter*)*user_data;
        if (message_data->m_MessageId == dmHashString32("start"))
        {
            dmParticle::StartEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        else if (message_data->m_MessageId == dmHashString32("restart"))
        {
            dmParticle::RestartEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        else if (message_data->m_MessageId == dmHashString32("stop"))
        {
            dmParticle::StopEmitter(emitter->m_World->m_ParticleContext, emitter->m_Emitter);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void RenderEmitterCallback(void* context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
    {
        EmitterWorld* world = (EmitterWorld*)context;

        uint32_t ro_count = world->m_RenderObjects.Size();
        world->m_RenderObjects.SetSize(ro_count + 1);
        dmRender::RenderObject* ro = &world->m_RenderObjects[ro_count];
        ro->m_Material = (dmRender::HMaterial)material;
        ro->m_Texture = (dmGraphics::HTexture)texture;
        ro->m_VertexStart = vertex_index;
        ro->m_VertexCount = vertex_count;
        ro->m_VertexBuffer = world->m_VertexBuffer;
        ro->m_VertexDeclaration = world->m_VertexDeclaration;
        ro->m_PrimitiveType = dmGraphics::PRIMITIVE_QUADS;
    }

    void RenderLineCallback(void* usercontext, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color)
    {
        dmRender::Line3D((dmRender::HRenderContext)usercontext, start, end, color, color);
    }
}
