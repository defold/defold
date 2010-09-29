#include "comp_emitter.h"

#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include <particle/particle.h>

#include <graphics/graphics_device.h>
#include <graphics/material.h>
#include <render_debug/debugrenderer.h>

#include "gamesys.h"

namespace dmGameSystem
{
    const uint8_t MAX_COUNT = 64;

    struct EmitterWorld;

    struct Emitter
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HEmitter m_Emitter;
        EmitterWorld* m_World;
    };

    struct EmitterWorld
    {
        dmParticle::HContext m_Context;
        dmArray<Emitter> m_Emitters;
    };

    dmGameObject::CreateResult CompEmitterNewWorld(void* context, void** world)
    {
        assert(context);
        EmitterContext* ctx = (EmitterContext*)context;
        EmitterWorld* emitter_world = new EmitterWorld();
        emitter_world->m_Context = dmParticle::CreateContext(ctx->m_ConfigFile);
        emitter_world->m_Emitters.SetCapacity(MAX_COUNT);
        *world = emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterDeleteWorld(void* context, void* world)
    {
        EmitterWorld* emitter_world = (EmitterWorld*)world;
        dmParticle::DestroyContext(emitter_world->m_Context);
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
            emitter.m_Emitter = dmParticle::CreateEmitter(w->m_Context, prototype);
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
        for (uint8_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            if (w->m_Emitters[i].m_Instance == instance)
            {
                dmParticle::DestroyEmitter(w->m_Context, w->m_Emitters[i].m_Emitter);
                w->m_Emitters.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        dmLogError("Destroyed emitter could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

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
            dmParticle::SetPosition(w->m_Context, emitter.m_Emitter, dmGameObject::GetWorldPosition(emitter.m_Instance));
            dmParticle::SetRotation(w->m_Context, emitter.m_Emitter, dmGameObject::GetWorldRotation(emitter.m_Instance));
        }
        EmitterContext* ctx = (EmitterContext*)context;
        dmParticle::Update(w->m_Context, update_context->m_DT, &ctx->m_RenderContext->m_View);

        dmParticle::Render(w->m_Context, ctx->m_RenderContext, RenderSetUpCallback, RenderTearDownCallback, RenderEmitterCallback);

        if (ctx->m_Debug)
        {
            dmGraphics::HContext gfx_context = ctx->m_RenderContext->m_GFXContext;

            dmGraphics::SetBlendFunc(gfx_context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
            dmGraphics::EnableState(gfx_context, dmGraphics::BLEND);

            dmParticle::DebugRender(w->m_Context, &ctx->m_RenderContext, RenderLineCallback);
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
            dmParticle::StartEmitter(emitter->m_World->m_Context, emitter->m_Emitter);
        }
        else if (message_data->m_MessageId == dmHashString32("restart"))
        {
            dmParticle::RestartEmitter(emitter->m_World->m_Context, emitter->m_Emitter);
        }
        else if (message_data->m_MessageId == dmHashString32("stop"))
        {
            dmParticle::StopEmitter(emitter->m_World->m_Context, emitter->m_Emitter);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void RenderSetUpCallback(void* render_context, float* vertex_buffer, uint32_t vertex_size)
    {
        dmRender::RenderContext* render_ctx = (dmRender::RenderContext*)render_context;
        dmGraphics::HContext gfx_context = render_ctx->m_GFXContext;

        dmGraphics::SetBlendFunc(gfx_context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(gfx_context, dmGraphics::BLEND);

        dmGraphics::SetDepthMask(gfx_context, 0);

        // positions
        dmGraphics::SetVertexStream(gfx_context, 0, 3, dmGraphics::TYPE_FLOAT, vertex_size, (void*)vertex_buffer);
        // uv's
        dmGraphics::SetVertexStream(gfx_context, 1, 2, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[3]);
        // alpha
        dmGraphics::SetVertexStream(gfx_context, 2, 1, dmGraphics::TYPE_FLOAT, vertex_size, (void*)&vertex_buffer[5]);
    }

    void RenderTearDownCallback(void* render_context)
    {
        dmGraphics::SetDepthMask(((dmRender::RenderContext*)render_context)->m_GFXContext, 1);
    }

    void RenderEmitterCallback(void* render_context, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
    {
        dmRender::RenderContext* render_ctx = (dmRender::RenderContext*)render_context;
        dmGraphics::HContext gfx_context = render_ctx->m_GFXContext;

        dmGraphics::SetVertexProgram(gfx_context, dmGraphics::GetMaterialVertexProgram((dmGraphics::HMaterial)material));
        dmGraphics::SetVertexConstantBlock(gfx_context, (const Vector4*)&render_ctx->m_ViewProj, 0, 4);
        dmGraphics::SetFragmentProgram(gfx_context, dmGraphics::GetMaterialFragmentProgram((dmGraphics::HMaterial)material));

        dmGraphics::SetTexture(gfx_context, (dmGraphics::HTexture)texture);

        dmGraphics::Draw(gfx_context, dmGraphics::PRIMITIVE_QUADS, vertex_index, vertex_count);
    }

    void RenderLineCallback(void* render_context, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color)
    {
        dmRender::RenderContext* render_ctx = (dmRender::RenderContext*)render_context;
        dmRenderDebug::Line(render_ctx->m_ViewProj, start, end, color);
    }
}
