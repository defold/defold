#include "comp_emitter.h"

#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include <particle/particle.h>

#include "gamesys.h"

namespace dmGameSystem
{
    const uint8_t MAX_COUNT = 64;

    struct World;

    struct Emitter
    {
        dmGameObject::HInstance m_Instance;
        dmParticle::HEmitter m_Emitter;
        World* m_World;
    };

    struct World
    {
        dmParticle::HContext m_Context;
        dmArray<Emitter> m_Emitters;
    };

    dmGameObject::CreateResult CompEmitterNewWorld(void* context, void** world)
    {
        assert(context);
        EmitterContext* ctx = (EmitterContext*)context;
        World* emitter_world = new World();
        emitter_world->m_Context = dmParticle::CreateContext(ctx->m_ConfigFile);
        emitter_world->m_Emitters.SetCapacity(MAX_COUNT);
        *world = emitter_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompEmitterDeleteWorld(void* context, void* world)
    {
        World* emitter_world = (World*)world;
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
        World* w = (World*)world;
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
        World* w = (World*)world;
        for (uint8_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            if (w->m_Emitters[i].m_Instance == instance)
            {
                w->m_Emitters.EraseSwap(i);
                return dmGameObject::CREATE_RESULT_OK;
            }
        }
        dmLogError("Destroyed emitter could not be found, something is fishy.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::UpdateResult CompEmitterUpdate(dmGameObject::HCollection collection,
            const dmGameObject::UpdateContext* update_context,
            void* world,
            void* context)
    {
        World* w = (World*)world;
        for (uint32_t i = 0; i < w->m_Emitters.Size(); ++i)
        {
            Emitter& emitter = w->m_Emitters[i];
            dmParticle::SetPosition(w->m_Context, emitter.m_Emitter, dmGameObject::GetWorldPosition(emitter.m_Instance));
            dmParticle::SetRotation(w->m_Context, emitter.m_Emitter, dmGameObject::GetWorldRotation(emitter.m_Instance));
        }
        EmitterContext* ctx = (EmitterContext*)context;
        dmParticle::Update(w->m_Context, update_context->m_DT, &ctx->m_RenderContext->m_View);

//        dmParticle::Render(m_Environment.m_ParticleSystemContext);

//        if (ctx->m_Debug)
//            dmParticle::DebugRender(m_Environment.m_ParticleSystemContext, &m_Environment.m_RenderContext.m_ViewProj, RenderLine);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompEmitterOnEvent(dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data)
    {
        Emitter* emitter = (Emitter*)*user_data;
        if (event_data->m_EventHash == dmHashString32("start"))
        {
            dmParticle::StartEmitter(emitter->m_World->m_Context, emitter->m_Emitter);
        }
        else if (event_data->m_EventHash == dmHashString32("stop"))
        {
            dmParticle::StopEmitter(emitter->m_World->m_Context, emitter->m_Emitter);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
