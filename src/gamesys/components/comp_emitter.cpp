#include "comp_emitter.h"

#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include <particle/particle.h>

#include <graphics/graphics_device.h>
#include <graphics/material.h>

#include <render/debug_renderer.h>

#include "gamesys.h"
#include "gamesys_private.h"

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
        dmArray<dmRender::HRenderObject> m_RenderObjects;
        EmitterContext* m_EmitterContext;
        dmParticle::HContext m_ParticleContext;
        float* m_VertexBuffer;
        uint32_t m_VertexSize;
    };

    struct ROUserData
    {
        EmitterWorld* m_World;
        dmGraphics::HMaterial m_Material;
        dmGraphics::HTexture m_Texture;
        uint32_t m_VertexCount;
        uint32_t m_VertexIndex;
    };

    dmGameObject::CreateResult CompEmitterNewWorld(void* context, void** world)
    {
        assert(context);
        EmitterContext* ctx = (EmitterContext*)context;
        EmitterWorld* emitter_world = new EmitterWorld();
        emitter_world->m_EmitterContext = ctx;
        emitter_world->m_ParticleContext = dmParticle::CreateContext(ctx->m_MaxEmitterCount, ctx->m_MaxParticleCount);
        emitter_world->m_Emitters.SetCapacity(MAX_COUNT);
        emitter_world->m_RenderObjects.SetCapacity(1000);
        *world = emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterDeleteWorld(void* context, void* world)
    {
        EmitterWorld* emitter_world = (EmitterWorld*)world;
        dmParticle::DestroyContext(emitter_world->m_ParticleContext);
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
        dmGraphics::HMaterial   m_Material;
        dmGraphics::HTexture    m_Texture;
        float*                  m_VertexData;
        uint32_t                m_VertexStride;
        uint32_t                m_VertexIndex;
        uint32_t                m_VertexCount;
    };

    void RenderSetUpCallback(void* render_context, float* vertex_buffer, uint32_t vertex_size);
    void RenderTearDownCallback(void* render_context);
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

        for (uint32_t i = 0; i < w->m_RenderObjects.Size(); ++i)
        {
            ROUserData* user_data = (ROUserData*)dmRender::GetUserData(w->m_RenderObjects[i]);
            delete user_data;
            dmRender::DeleteRenderObject(w->m_RenderObjects[i]);
        }
        w->m_RenderObjects.SetSize(0);

        dmParticle::Update(w->m_ParticleContext, update_context->m_DT);
        dmParticle::Render(w->m_ParticleContext, w, RenderSetUpCallback, 0x0, RenderEmitterCallback);
        for (uint32_t i = 0; i < w->m_RenderObjects.Size(); ++i)
        {
            dmRender::AddToRender(ctx->m_RenderContext, w->m_RenderObjects[i]);
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

    void RenderSetUpCallback(void* context, float* vertex_buffer, uint32_t vertex_size)
    {
        EmitterWorld* world = (EmitterWorld*)context;
        world->m_VertexBuffer = vertex_buffer;
        world->m_VertexSize = vertex_size;
    }

    void RenderEmitterCallback(void* context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
    {
        EmitterWorld* world = (EmitterWorld*)context;

        dmRender::HRenderObject ro = dmRender::NewRenderObject(world->m_EmitterContext->m_ParticleRenderType, (dmGraphics::HMaterial)material, 0x0);
        ROUserData* user_data = new ROUserData();
        user_data->m_World = world;
        user_data->m_Material = (dmGraphics::HMaterial)material;
        user_data->m_Texture = (dmGraphics::HTexture)texture;
        user_data->m_VertexCount = vertex_count;
        user_data->m_VertexIndex = vertex_index;
        dmRender::SetUserData(ro, (void*)user_data);
        world->m_RenderObjects.Push(ro);
    }

    void RenderLineCallback(void* usercontext, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color)
    {
        dmRender::Line3D((dmRender::HRenderContext)usercontext, start, end, color, color);
    }

    void RenderTypeParticleDraw(dmRender::HRenderContext render_context, dmRender::HRenderObject ro, uint32_t count)
    {
        dmGraphics::HContext gfx_context = dmRender::GetGraphicsContext(render_context);

        ROUserData* user_data = (ROUserData*)dmRender::GetUserData(ro);

        float* vertex_buffer = user_data->m_World->m_VertexBuffer;
        uint32_t vertex_size = user_data->m_World->m_VertexSize;
        // positions
        dmGraphics::SetVertexStream(gfx_context, 0, 3, dmGraphics::TYPE_FLOAT, vertex_size, (void*)vertex_buffer);
        // uv's
        dmGraphics::SetVertexStream(gfx_context, 1, 2, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[3]);
        // alpha
        dmGraphics::SetVertexStream(gfx_context, 2, 1, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[5]);

        dmGraphics::SetVertexConstantBlock(gfx_context, (const Vector4*)dmRender::GetViewProjectionMatrix(render_context), 0, 4);

        dmGraphics::SetTexture(gfx_context, user_data->m_Texture);

        dmGraphics::Draw(gfx_context, dmGraphics::PRIMITIVE_QUADS, user_data->m_VertexIndex, user_data->m_VertexCount);
    }
}
