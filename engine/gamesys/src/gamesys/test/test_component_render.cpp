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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlib/condition_variable.h>
#include <dlib/hash.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <gameobject/comp_anim.h>
#include <gameobject/component.h>
#include <gameobject/gameobject_private.h>
#include <gameobject/gameobject.h>
#include <render/render_execution.h>
#include <render/render_frame_queue.h>
#include <resource/resource.h>
#include <script/script.h>
#include <testmain/testmain.h>

#include "gamesys/component_render.h"
#include "gamesys/component_render_state_private.h"
#include "gamesys/gamesys.h"
#include "gamesys/gamesys_context_private.h"

namespace
{
    enum EventType
    {
        EVENT_TYPE_GAME_CREATE,
        EVENT_TYPE_GAME_DELETE,
        EVENT_TYPE_UPDATE,
        EVENT_TYPE_LATE_UPDATE,
        EVENT_TYPE_CAPTURE,
        EVENT_TYPE_LEGACY_RENDER,
        EVENT_TYPE_FRAME_RELEASE,
        EVENT_TYPE_POST_UPDATE,
        EVENT_TYPE_RENDER_CREATE,
        EVENT_TYPE_RENDER_CONSUME,
        EVENT_TYPE_RENDER_DELETE,
    };

    struct Event
    {
        uint64_t  m_RenderWorld;
        EventType m_EventType;
        uint32_t  m_ComponentIndex;
        uint32_t  m_World;
        uint32_t  m_Value;
        float     m_X;
    };

    // Shared test state: records lifecycle events and ownership checks, injects failures,
    // and coordinates game/render callbacks to test ordering without timing assumptions.
    struct Probe
    {
        Event                                   m_Events[128];
        dmRender::HRenderExecution              m_Execution;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        uint32_t                                m_EventCount;
        uint32_t                                m_NextWorld;
        // Use full bytes, not bitfields: runtime flag writes must not read/modify/write
        // storage shared with concurrent, unlocked configuration reads.
        // Configuration stays immutable while the render executor is running.
        uint8_t                                 m_FailRenderInit;
        uint8_t                                 m_FailRenderConsume;
        uint8_t                                 m_HoldConsume;
        // All concurrent accesses to these flags hold m_Mutex.
        uint8_t                                 m_WrongOwner;
        uint8_t                                 m_ConsumeEntered;
        uint8_t                                 m_ReleaseConsume;
    };

    struct ArtificialType
    {
        Probe*   m_Probe;
        dmGameSystem::HComponentRenderContext m_CaptureOwner;
        uint32_t m_Index;
        uint8_t  m_FailCapture : 1;
        uint8_t  m_ObserveRetirement : 1;
        uint8_t  m_DeleteCollectionOnPostUpdate : 1;
        uint8_t  m_FailPostUpdate : 1;
        uint8_t  m_NestedCaptureRejected : 1;
        uint8_t  m_NestedRenderRejected : 1;
    };

    struct ArtificialWorld
    {
        dmGameObject::HInstance   m_Instance;
        ArtificialType*           m_Type;
        dmRender::HRenderFrame    m_LastCapturedFrame;
        dmGameObject::HCollection m_ChildCollection;
        uint32_t                  m_Id;
        uint32_t                  m_Value;
        uint8_t                   m_ChildEnabled : 1;
    };

    struct ArtificialInput
    {
        uint32_t m_Type;
        uint32_t m_World;
        uint32_t m_Value;
        float    m_X;
    };

    struct ArtificialRenderer
    {
        dmGameSystem::ComponentRenderWorldInfo m_Info;
    };

    struct ComponentRenderFixture
    {
        Probe                                 m_Probe;
        ArtificialType                        m_Types[2];
        dmResource::HFactory                  m_Factory;
        dmScript::HContext                    m_Script;
        dmGameObject::HRegister               m_Register;
        dmGameSystem::HContext                m_Context;
        dmGameSystem::HComponentRenderContext m_Render;
        dmRender::HRenderExecution            m_Execution;
        uint32_t                              m_TypeCount;
    };

    static void RecordEvent(Probe* probe, EventType event_type, uint32_t component_index, uint32_t world,
        uint32_t value, float x, dmRender::ExecutionOwner owner, uint64_t render_world = 0)
    {
        DM_MUTEX_SCOPED_LOCK(probe->m_Mutex);
        assert(probe->m_EventCount < sizeof(probe->m_Events) / sizeof(probe->m_Events[0]));
        Event& event = probe->m_Events[probe->m_EventCount++];
        event.m_EventType = event_type;
        event.m_ComponentIndex = component_index;
        event.m_World = world;
        event.m_Value = value;
        event.m_X = x;
        event.m_RenderWorld = render_world;
        probe->m_WrongOwner |= !dmRender::IsRenderExecutionOwner(probe->m_Execution, owner);
    }

    static uint32_t CountEvents(Probe* probe, EventType event_type)
    {
        DM_MUTEX_SCOPED_LOCK(probe->m_Mutex);
        uint32_t count = 0;
        for (uint32_t i = 0; i < probe->m_EventCount; ++i)
        {
            count += probe->m_Events[i].m_EventType == event_type;
        }
        return count;
    }

    static Event GetEvent(Probe* probe, EventType event_type, uint32_t occurrence)
    {
        DM_MUTEX_SCOPED_LOCK(probe->m_Mutex);
        for (uint32_t i = 0; i < probe->m_EventCount; ++i)
        {
            if (probe->m_Events[i].m_EventType == event_type && occurrence-- == 0)
            {
                return probe->m_Events[i];
            }
        }
        assert(false);
        Event empty = {};
        return empty;
    }

    static dmResource::Result CreateUnusedResource(const dmResource::ResourceCreateParams*)
    {
        return dmResource::RESULT_UNKNOWN_ERROR;
    }

    static dmResource::Result DestroyUnusedResource(const dmResource::ResourceDestroyParams*)
    {
        return dmResource::RESULT_OK;
    }

    static dmGameObject::CreateResult CreateGameWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ArtificialType* type = (ArtificialType*)params.m_Context;
        ArtificialWorld* world = (ArtificialWorld*)calloc(1, sizeof(ArtificialWorld));
        world->m_Type = type;
        world->m_Id = ++type->m_Probe->m_NextWorld;
        world->m_Value = world->m_Id * 10;
        *params.m_World = world;
        RecordEvent(type->m_Probe, EVENT_TYPE_GAME_CREATE, type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult DeleteGameWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_GAME_DELETE, world->m_Type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);
        memset(world, 0xdd, sizeof(*world));
        free(world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult AddToUpdate(const dmGameObject::ComponentAddToUpdateParams&)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult Update(const dmGameObject::ComponentsUpdateParams& params,
        dmGameObject::ComponentsUpdateResult&)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        ++world->m_Value;
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_UPDATE, world->m_Type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult LateUpdate(const dmGameObject::ComponentsUpdateParams& params,
        dmGameObject::ComponentsUpdateResult& result)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        world->m_Value += 100;
        if (world->m_Instance)
        {
            dmGameObject::SetPosition(world->m_Instance, dmVMath::Point3(7.0f, 0.0f, 0.0f));
            result.m_TransformsUpdated = true;
        }
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_LATE_UPDATE, world->m_Type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult PostUpdate(const dmGameObject::ComponentsPostUpdateParams& params)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        world->m_Value += 1000;
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_POST_UPDATE, world->m_Type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);
        if (world->m_Type->m_DeleteCollectionOnPostUpdate)
            dmGameObject::DeleteCollection(params.m_Collection);
        return world->m_Type->m_FailPostUpdate ? dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR : dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult LegacyRender(const dmGameObject::ComponentsRenderParams& params)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_LEGACY_RENDER, world->m_Type->m_Index, world->m_Id, world->m_Value, 0, dmRender::EXECUTION_OWNER_GAME);

        if (world->m_ChildCollection && world->m_ChildEnabled && !dmGameObject::Render(world->m_ChildCollection))
        {
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void ReleaseInputReference(void*, void* resource)
    {
        // Type storage belongs to gamesys and outlives game worlds and frame references.
        ArtificialType* type = (ArtificialType*)resource;
        RecordEvent(type->m_Probe, EVENT_TYPE_FRAME_RELEASE, type->m_Index, 0, 0, 0, dmRender::EXECUTION_OWNER_GAME);
    }

    static dmRender::RenderFrameResult Capture(const dmGameSystem::ComponentRenderCaptureParams& params)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        world->m_LastCapturedFrame = params.m_Frame;
        ArtificialInput input;
        input.m_Type = world->m_Type->m_Index;
        input.m_World = world->m_Id;
        input.m_Value = world->m_Value;
        input.m_X = world->m_Instance ? dmGameObject::GetWorldPosition(world->m_Instance).getX() : 0.0f;
        RecordEvent(world->m_Type->m_Probe, EVENT_TYPE_CAPTURE, input.m_Type, input.m_World, input.m_Value, input.m_X, dmRender::EXECUTION_OWNER_GAME);

        uint32_t table;
        dmRender::RenderFrameResult result = dmRender::CopyRenderFrameTable(params.m_Frame, input.m_Type, 1, sizeof(input), 4, &input, &table);

        if (result != dmRender::RENDER_FRAME_OK)
        {
            return result;
        }
        if (world->m_Type->m_ObserveRetirement)
        {
            result = dmRender::AddRenderFrameReference(params.m_Frame, world->m_Type, ReleaseInputReference, 0);
            if (result != dmRender::RENDER_FRAME_OK)
                return result;
        }
        return world->m_Type->m_FailCapture ? dmRender::RENDER_FRAME_INVALID_ARGUMENT : dmRender::RENDER_FRAME_OK;
    }

    static dmRender::RenderFrameResult CaptureWithNestedAttempt(const dmGameSystem::ComponentRenderCaptureParams& params)
    {
        ArtificialType* type = (ArtificialType*)params.m_Context;
        type->m_NestedCaptureRejected = !dmGameSystem::CaptureComponentRenderData(type->m_CaptureOwner, params.m_Collection, params.m_Frame);
        type->m_NestedRenderRejected = !dmGameSystem::RenderComponents(type->m_CaptureOwner, params.m_Collection);
        return Capture(params);
    }

    static dmRender::RenderFrameResult InitializeRenderer(void* context, const void* data, uint32_t size, void** state)
    {
        Probe* probe = (Probe*)context;
        assert(size == sizeof(dmGameSystem::ComponentRenderWorldInfo));
        ArtificialRenderer* renderer = (ArtificialRenderer*)malloc(sizeof(ArtificialRenderer));
        renderer->m_Info = *(const dmGameSystem::ComponentRenderWorldInfo*)data;
        *state = renderer;
        RecordEvent(probe, EVENT_TYPE_RENDER_CREATE, renderer->m_Info.m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        return probe->m_FailRenderInit ? dmRender::RENDER_FRAME_INVALID_ARGUMENT : dmRender::RENDER_FRAME_OK;
    }

    static dmRender::RenderFrameResult ConsumeRenderer(void* context, void* state,
        const dmRender::RenderFrameView& frame, uint32_t first_table, uint32_t table_count)
    {
        Probe* probe = (Probe*)context;
        dmMutex::Lock(probe->m_Mutex);
        if (probe->m_HoldConsume)
        {
            probe->m_ConsumeEntered = true;
            dmConditionVariable::Broadcast(probe->m_Changed);
            while (!probe->m_ReleaseConsume)
            {
                dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
            }
        }
        dmMutex::Unlock(probe->m_Mutex);
        ArtificialRenderer* renderer = (ArtificialRenderer*)state;
        assert(table_count == 1);
        const ArtificialInput* input = (const ArtificialInput*)dmRender::GetRenderFrameTableData(frame, first_table);
        assert(frame.m_Tables[first_table].m_Count == 1);
        assert(frame.m_Tables[first_table].m_Stride == sizeof(*input));
        assert(input->m_Type == renderer->m_Info.m_ComponentIndex);

        RecordEvent((Probe*)context, EVENT_TYPE_RENDER_CONSUME, input->m_Type, input->m_World, input->m_Value, input->m_X, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        return ((Probe*)context)->m_FailRenderConsume ? dmRender::RENDER_FRAME_INVALID_ARGUMENT : dmRender::RENDER_FRAME_OK;
    }

    static void DeleteRenderer(void* context, void* state)
    {
        ArtificialRenderer* renderer = (ArtificialRenderer*)state;
        RecordEvent((Probe*)context, EVENT_TYPE_RENDER_DELETE, renderer->m_Info.m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        free(renderer);
    }

    static dmGameSystem::ComponentRenderType MakeRenderType(Probe* probe)
    {
        dmGameSystem::ComponentRenderType type;
        type.m_Capture = Capture;
        type.m_RenderCallbacks.m_Initialize = InitializeRenderer;
        type.m_RenderCallbacks.m_Consume = ConsumeRenderer;
        type.m_RenderCallbacks.m_Destroy = DeleteRenderer;
        type.m_RenderContext = probe;
        return type;
    }

    static dmGameObject::Result RegisterRenderer(ComponentRenderFixture* fixture, uint32_t index)
    {
        dmGameSystem::ComponentRenderType type = MakeRenderType(&fixture->m_Probe);
        return dmGameSystem::RegisterComponentRenderType(fixture->m_Render, index, type);
    }

    static dmGameObject::ComponentType MakeGameType(ComponentRenderFixture* fixture, uint32_t type_slot, const char* name)
    {
        ArtificialType* context = &fixture->m_Types[type_slot];
        context->m_Probe = &fixture->m_Probe;
        context->m_Index = dmGameObject::GetNumComponentTypes(fixture->m_Register);

        dmResource::Result resource_result = dmResource::RegisterType(fixture->m_Factory, name, 0, 0, CreateUnusedResource, 0, DestroyUnusedResource, 0);
        assert(resource_result == dmResource::RESULT_OK);

        HResourceType resource_type;
        resource_result = dmResource::GetTypeFromExtension(fixture->m_Factory, name, &resource_type);
        assert(resource_result == dmResource::RESULT_OK);
        (void)resource_result;

        dmGameObject::ComponentType type;
        type.m_Name = name;
        type.m_ResourceType = resource_type;
        type.m_Context = context;
        type.m_NewWorldFunction = CreateGameWorld;
        type.m_DeleteWorldFunction = DeleteGameWorld;
        type.m_AddToUpdateFunction = AddToUpdate;
        type.m_UpdateFunction = Update;
        type.m_LateUpdateFunction = LateUpdate;
        type.m_RenderFunction = LegacyRender;
        type.m_PostUpdateFunction = PostUpdate;
        type.m_UpdateOrderPrio = (uint16_t)(10 - type_slot);
        return type;
    }

    static void NewComponentRenderFixture(ComponentRenderFixture* fixture, dmRender::ExecutionMode mode, uint32_t type_count, bool register_render = true)
    {
        memset(fixture, 0, sizeof(*fixture));
        dmRender::ExecutionSelection selection;
        fixture->m_Execution = dmRender::NewRenderExecution(mode, true, &selection);
        assert(fixture->m_Execution && selection.m_ExecutionMode == mode);

        fixture->m_Probe.m_Execution = fixture->m_Execution;
        fixture->m_Probe.m_Mutex = dmMutex::New();
        fixture->m_Probe.m_Changed = dmConditionVariable::New();

        dmResource::NewFactoryParams factory_params;
        factory_params.m_MaxResources = 16;
        factory_params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        fixture->m_Factory = dmResource::NewFactory(&factory_params, ".");
        assert(fixture->m_Factory);

        dmScript::ContextParams script_params = {};
        fixture->m_Script = dmScript::NewContext(script_params);
        dmScript::Initialize(fixture->m_Script);
        fixture->m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(fixture->m_Register, fixture->m_Script);

        dmGameSystem::ContextParams context_params = {};
        context_params.m_Factory = fixture->m_Factory;
        context_params.m_Register = fixture->m_Register;
        context_params.m_ScriptContext = fixture->m_Script;
        const dmRender::RenderFrameLimits limits = { 4096, 16, 16 };
        fixture->m_Context = dmGameSystem::NewContext(context_params, fixture->m_Execution, limits);
        assert(fixture->m_Context);
        fixture->m_Render = fixture->m_Context->m_ComponentRenderContext;
        fixture->m_TypeCount = type_count;

        const char* names[] = { "artificial_a", "artificial_b" };
        for (uint32_t i = 0; i < type_count; ++i)
        {
            dmGameObject::ComponentType type = MakeGameType(fixture, i, names[i]);
            dmGameSystem::ComponentRenderType render_type = MakeRenderType(&fixture->m_Probe);
            dmGameObject::Result game_result = dmGameSystem::RegisterComponentType(fixture->m_Context, type,
                register_render ? &render_type : 0);
            assert(game_result == dmGameObject::RESULT_OK);
            (void)game_result;
        }
        // Even empty game objects cancel property animations during deletion. Use
        // the real animation world rather than bypassing that production cleanup.
        dmResource::Result resource_result = dmResource::RegisterType(fixture->m_Factory, "animc", 0, 0, CreateUnusedResource, 0, DestroyUnusedResource, 0);
        assert(resource_result == dmResource::RESULT_OK);

        HResourceType animation_resource;
        resource_result = dmResource::GetTypeFromExtension(fixture->m_Factory, "animc", &animation_resource);
        assert(resource_result == dmResource::RESULT_OK);

        (void)resource_result;
        dmGameObject::ComponentType animation;
        animation.m_Name = "animc";
        animation.m_ResourceType = animation_resource;
        animation.m_NewWorldFunction = dmGameObject::CompAnimNewWorld;
        animation.m_DeleteWorldFunction = dmGameObject::CompAnimDeleteWorld;
        animation.m_AddToUpdateFunction = dmGameObject::CompAnimAddToUpdate;
        animation.m_UpdateFunction = dmGameObject::CompAnimUpdate;
        dmGameObject::Result game_result = dmGameSystem::RegisterComponentType(fixture->m_Context, animation);
        assert(game_result == dmGameObject::RESULT_OK);

        (void)game_result;
        dmGameObject::SortComponentTypes(fixture->m_Register);
    }

    static dmGameObject::HCollection NewCollection(ComponentRenderFixture* fixture, const char* name)
    {
        return dmGameObject::NewCollection(name, fixture->m_Factory, fixture->m_Register, 8, 0);
    }

    static ArtificialWorld* GetWorld(dmGameObject::HCollection collection, uint32_t type = 0)
    {
        return (ArtificialWorld*)dmGameObject::GetWorld(collection, type);
    }

    static void DeleteCollection(ComponentRenderFixture* fixture, dmGameObject::HCollection collection)
    {
        dmGameObject::DeleteCollection(collection);
        dmGameObject::PostUpdate(fixture->m_Register);
    }

    static void DeleteComponentRenderFixture(ComponentRenderFixture* fixture)
    {
        dmGameObject::DeleteCollections(fixture->m_Register);
        if (fixture->m_Render)
        {
            // Failure-injection tests still complete lifetime cleanup before
            // deleting the context, regardless of the earlier component error.
            bool finalized = dmGameSystem::Finalize(fixture->m_Context);
            assert(finalized);
            (void)finalized;
            fixture->m_Render = 0;
        }
        dmScript::Finalize(fixture->m_Script);
        dmScript::DeleteContext(fixture->m_Script);
        dmGameObject::DeleteRegister(fixture->m_Register);
        dmResource::DeleteFactory(fixture->m_Factory);
        dmGameSystem::DeleteContext(fixture->m_Context);
        dmRender::DeleteRenderExecution(fixture->m_Execution);
        dmConditionVariable::Delete(fixture->m_Probe.m_Changed);
        dmMutex::Delete(fixture->m_Probe.m_Mutex);
    }

    // Tests without a simulation step still reserve before capture. Keep PostUpdate
    // separate so they can inspect accepted inputs before retirement/game cleanup.
    static bool BeginAndRenderComponents(dmGameSystem::HContext context, dmGameObject::HCollection collection)
    {
        return dmGameSystem::BeginFrame(context) && dmGameSystem::Render(context, collection);
    }

    // Intercept the generic owner seam to inject a creation failure while keeping the
    // real gamesys association/lifetime cleanup for earlier worlds in the collection.
    struct WorldObserverProbe
    {
        dmGameObject::ComponentWorldCallbacks m_Next;
        dmGameObject::HCollection             m_CreatedCollection;
        dmGameObject::HCollection             m_DeletedCollection;
        uint32_t                              m_DeletedCount;
    };

    static bool ObserveWorldCreated(void* context, const dmGameObject::ComponentWorldCreateParams& params)
    {
        WorldObserverProbe* probe = (WorldObserverProbe*) context;
        probe->m_CreatedCollection = params.m_Collection;
        if (params.m_ComponentIndex == 1)
        {
            return false;
        }
        return probe->m_Next.m_OnWorldCreated(probe->m_Next.m_Context, params);
    }

    static void ObserveWorldDeleted(void* context, dmGameObject::HCollection collection, uint32_t component_index)
    {
        WorldObserverProbe* probe = (WorldObserverProbe*) context;
        probe->m_DeletedCollection = collection;
        ++probe->m_DeletedCount;
        probe->m_Next.m_OnWorldDeleted(probe->m_Next.m_Context, collection, component_index);
    }

    static dmGameObject::UpdateResult ObserveWorldRender(void* context, uint32_t component_index, const dmGameObject::ComponentsRenderParams& params, dmGameObject::ComponentsRender render)
    {
        WorldObserverProbe* probe = (WorldObserverProbe*) context;
        return probe->m_Next.m_Render(probe->m_Next.m_Context, component_index, params, render);
    }

    // Step commands use condition variables so tests can keep an accepted frame Ready
    // until after game-world deletion without sleep-based scheduling assumptions.
    struct Executor
    {
        ComponentRenderFixture*                 m_Fixture;
        dmThread::Thread                        m_Thread;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        uint32_t                                m_Requested;
        uint32_t                                m_Completed;
        dmRender::RenderFramePumpResult         m_Result;
        // All executor control flags are protected by m_Mutex.
        uint8_t                                 m_Bound : 1;
        uint8_t                                 m_Consume : 1;
        uint8_t                                 m_Exit : 1;
    };

    static void JoinExecutor(void* context);

    static void RunExecutor(void* context)
    {
        Executor* executor = (Executor*)context;
        bool bound = dmRender::BindRenderExecutionOwner(executor->m_Fixture->m_Execution);
        assert(bound);
        (void)bound;
        dmMutex::Lock(executor->m_Mutex);
        executor->m_Bound = true;
        dmConditionVariable::Broadcast(executor->m_Changed);
        for (;;)
        {
            while (!executor->m_Exit && executor->m_Requested == executor->m_Completed)
            {
                dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
            }

            if (executor->m_Exit)
            {
                break;
            }

            bool consume = executor->m_Consume;
            dmMutex::Unlock(executor->m_Mutex);
            dmRender::RenderFramePumpResult result = dmRender::PumpRenderFrameQueue( dmGameSystem::GetComponentRenderQueue(executor->m_Fixture->m_Render), consume);
            dmMutex::Lock(executor->m_Mutex);

            executor->m_Result = result;
            ++executor->m_Completed;
            dmConditionVariable::Broadcast(executor->m_Changed);
        }
        dmMutex::Unlock(executor->m_Mutex);
        dmRender::ReleaseRenderExecutionOwner(executor->m_Fixture->m_Execution);
    }

    static void StartExecutor(Executor* executor, ComponentRenderFixture* fixture)
    {
        memset(executor, 0, sizeof(*executor));
        executor->m_Fixture = fixture;
        executor->m_Mutex = dmMutex::New();
        executor->m_Changed = dmConditionVariable::New();
        dmRender::RenderFrameResult result = dmRender::SetRenderFrameExecutorShutdown(
            dmGameSystem::GetComponentRenderQueue(fixture->m_Render), JoinExecutor, executor);
        assert(result == dmRender::RENDER_FRAME_OK);
        (void)result;
        executor->m_Thread = dmThread::New(RunExecutor, 0x80000, executor, "component-render-test");

        DM_MUTEX_SCOPED_LOCK(executor->m_Mutex);
        while (!executor->m_Bound)
        {
            dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
        }
    }

    static void RequestPump(Executor* executor, bool consume)
    {
        DM_MUTEX_SCOPED_LOCK(executor->m_Mutex);
        assert(executor->m_Requested == executor->m_Completed);
        executor->m_Consume = consume;
        ++executor->m_Requested;
        dmConditionVariable::Broadcast(executor->m_Changed);
    }

    static dmRender::RenderFramePumpResult WaitPump(Executor* executor)
    {
        DM_MUTEX_SCOPED_LOCK(executor->m_Mutex);
        while (executor->m_Requested != executor->m_Completed)
        {
            dmConditionVariable::Wait(executor->m_Changed, executor->m_Mutex);
        }
        return executor->m_Result;
    }

    static dmRender::RenderFramePumpResult Pump(Executor* executor, bool consume)
    {
        RequestPump(executor, consume);
        return WaitPump(executor);
    }

    static void JoinExecutor(void* context)
    {
        Executor* executor = (Executor*)context;
        dmMutex::Lock(executor->m_Mutex);
        executor->m_Exit = true;
        dmConditionVariable::Broadcast(executor->m_Changed);
        dmMutex::Unlock(executor->m_Mutex);
        dmThread::Join(executor->m_Thread);
        dmConditionVariable::Delete(executor->m_Changed);
        dmMutex::Delete(executor->m_Mutex);
    }

    static bool HasThreads()
    {
        dmRender::ExecutionCapabilities capabilities = dmRender::GetExecutionCapabilities(true);
        return capabilities.m_BuildEnabled && capabilities.m_ThreadsSupported;
    }

    // A native owner that services queue notifications automatically. Only setup and
    // teardown differ from inline execution; producer tests use the same gamesys API.
    struct AutomaticExecutor
    {
        dmRender::RenderFrameQueueStatus m_ShutdownStatus;
        ComponentRenderFixture*          m_Fixture;
        dmThread::Thread                 m_Thread;
        uint32_t                         m_ShutdownCalls;
        uint32_t                         m_ShutdownWorldCount;
    };

    static void JoinAutomaticExecutor(void* context)
    {
        AutomaticExecutor* executor = (AutomaticExecutor*)context;
        dmThread::Join(executor->m_Thread);
        ++executor->m_ShutdownCalls;
        // Finalize must keep queue and callback storage alive through the join.
        executor->m_ShutdownStatus = dmRender::GetRenderFrameQueueStatus(
            dmGameSystem::GetComponentRenderQueue(executor->m_Fixture->m_Render));
        executor->m_ShutdownWorldCount = dmGameSystem::GetComponentRenderWorldCount(executor->m_Fixture->m_Render);
    }

    static void RunAutomaticExecutor(void* context)
    {
        AutomaticExecutor* executor = (AutomaticExecutor*)context;
        ComponentRenderFixture* fixture = executor->m_Fixture;
        bool bound = dmRender::BindRenderExecutionOwner(fixture->m_Execution);
        assert(bound);
        (void)bound;

        dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture->m_Render);
        for (;;)
        {
            dmRender::RenderFramePumpResult result = dmRender::PumpRenderFrameQueue(queue, true);
            if (result == dmRender::RENDER_FRAME_PUMP_DRAINED)
                break;
            if (result == dmRender::RENDER_FRAME_PUMP_IDLE || result == dmRender::RENDER_FRAME_PUMP_BUSY)
                dmRender::WaitForRenderFrameWork(queue);
        }

        dmRender::ReleaseRenderExecutionOwner(fixture->m_Execution);
    }

    static void StartAutomaticExecutor(AutomaticExecutor* executor, ComponentRenderFixture* fixture)
    {
        executor->m_Fixture = fixture;
        executor->m_ShutdownCalls = 0;
        dmRender::RenderFrameResult result = dmRender::SetRenderFrameExecutorShutdown(
            dmGameSystem::GetComponentRenderQueue(fixture->m_Render), JoinAutomaticExecutor, executor);
        assert(result == dmRender::RENDER_FRAME_OK);
        (void)result;
        executor->m_Thread = dmThread::New(RunAutomaticExecutor, 0x80000, executor, "component-auto-test");
    }
}

// Verify identical producer calls preserve ordered snapshots in both modes, including an unpublished shutdown frame.
TEST(ComponentRender, ModeIndependentFramePipeline)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1);
        AutomaticExecutor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
            StartAutomaticExecutor(&executor, &fixture);

        dmGameObject::HCollection collection = NewCollection(&fixture, "common_frame_pipeline");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(dmGameObject::Init(collection));
        dmGameObject::UpdateContext update;
        update.m_DT = 1.0f / 60.0f;

        for (uint32_t frame = 0; frame < 3; ++frame)
        {
            ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
            ASSERT_TRUE(dmGameObject::Update(collection, &update));
            ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
            ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        }

        // Shutdown may interrupt a non-drawing frame. Only its unpublished reservation
        // is cancelled; all earlier accepted snapshots still consume and retire.
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        DeleteCollection(&fixture, collection);
        ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
        fixture.m_Render = 0;

        ASSERT_EQ(3u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
        ASSERT_EQ(3u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(111u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_Value);
        ASSERT_EQ(1212u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_Value);
        ASSERT_EQ(2313u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 2).m_Value);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_EQ((dmGameSystem::HComponentRenderContext)0, fixture.m_Context->m_ComponentRenderContext);
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(1u, executor.m_ShutdownCalls);
            ASSERT_EQ(0u, executor.m_ShutdownWorldCount);
            ASSERT_EQ((uint64_t)3, executor.m_ShutdownStatus.m_Published);
            ASSERT_EQ((uint64_t)3, executor.m_ShutdownStatus.m_Consumed);
            ASSERT_EQ((uint64_t)1, executor.m_ShutdownStatus.m_Cancelled);
            ASSERT_TRUE(executor.m_ShutdownStatus.m_Stopping);
        }
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify skipped draws return their capture slots and renderer cleanup wakes without publishing any frame in either mode.
TEST(ComponentRender, NonDrawingFramesReleaseSlotsAndServiceWorlds)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1);
        AutomaticExecutor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
            StartAutomaticExecutor(&executor, &fixture);

        dmGameObject::HCollection collection = NewCollection(&fixture, "common_non_drawing_frame");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        for (uint32_t frame = 0; frame < 2; ++frame)
        {
            ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
            ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        }

        dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
        ASSERT_EQ((uint64_t)0, status.m_Published);
        ASSERT_EQ((uint64_t)0, status.m_Consumed);
        ASSERT_EQ((uint64_t)2, status.m_Cancelled);
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[0]);
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[1]);

        DeleteCollection(&fixture, collection);
        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, 0));
        ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
        fixture.m_Render = 0;

        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_EQ((dmGameSystem::HComponentRenderContext)0, fixture.m_Context->m_ComponentRenderContext);
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify the no-opt-in engine path creates no frame queue and retains ordinary collection rendering.
TEST(ComponentRender, NoRegisteredTypesAllocateNoFrames)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false);
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    dmGameObject::HCollection collection = NewCollection(&fixture, "no_render_callbacks");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    DeleteCollection(&fixture, collection);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    DeleteComponentRenderFixture(&fixture);
}

// Verify frame references retire before game cleanup and renderer service, including nonfatal game PostUpdate errors.
TEST(ComponentRender, PostUpdateOwnsRetirementCleanupAndService)
{
    for (uint32_t fail_post_update = 0; fail_post_update < 2; ++fail_post_update)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
        fixture.m_Types[0].m_ObserveRetirement = true;
        fixture.m_Types[0].m_DeleteCollectionOnPostUpdate = true;
        fixture.m_Types[0].m_FailPostUpdate = fail_post_update != 0;

        dmGameObject::HCollection collection = NewCollection(&fixture, "post_update_cleanup_order");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));

        const uint32_t first_cleanup_event = fixture.m_Probe.m_EventCount;
        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));

        ASSERT_EQ(first_cleanup_event + 4, fixture.m_Probe.m_EventCount);
        ASSERT_EQ(EVENT_TYPE_FRAME_RELEASE, fixture.m_Probe.m_Events[first_cleanup_event].m_EventType);
        ASSERT_EQ(EVENT_TYPE_POST_UPDATE, fixture.m_Probe.m_Events[first_cleanup_event + 1].m_EventType);
        ASSERT_EQ(EVENT_TYPE_GAME_DELETE, fixture.m_Probe.m_Events[first_cleanup_event + 2].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_DELETE, fixture.m_Probe.m_Events[first_cleanup_event + 3].m_EventType);
        ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

        dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[0]);
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[1]);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify paired registration rejects malformed callbacks and duplicates without leaking a type, binding or frame queue.
TEST(ComponentRender, PairedRegistrationRollsBack)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false);
    const uint32_t original_count = dmGameObject::GetNumComponentTypes(fixture.m_Register);
    dmGameObject::ComponentType type = MakeGameType(&fixture, 1, "paired_component");
    dmGameSystem::ComponentRenderType render_type = MakeRenderType(&fixture.m_Probe);
    render_type.m_RenderCallbacks.m_Consume = 0;

    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION,
        dmGameSystem::RegisterComponentType(fixture.m_Context, type, &render_type));
    ASSERT_EQ(original_count, dmGameObject::GetNumComponentTypes(fixture.m_Register));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));

    render_type = MakeRenderType(&fixture.m_Probe);
    const dmGameObject::ComponentType duplicate = *dmGameObject::GetComponentType(fixture.m_Register, 0);
    ASSERT_EQ(dmGameObject::RESULT_ALREADY_REGISTERED,
        dmGameSystem::RegisterComponentType(fixture.m_Context, duplicate, &render_type));
    ASSERT_EQ(original_count, dmGameObject::GetNumComponentTypes(fixture.m_Register));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));

    ASSERT_EQ(dmGameObject::RESULT_OK,
        dmGameSystem::RegisterComponentType(fixture.m_Context, type, &render_type));
    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
    ASSERT_NE((dmRender::HRenderFrameQueue)0, queue);
    ASSERT_EQ(original_count + 1, dmGameObject::GetNumComponentTypes(fixture.m_Register));

    ASSERT_EQ(dmGameObject::RESULT_ALREADY_REGISTERED,
        dmGameSystem::RegisterComponentType(fixture.m_Context, type, &render_type));
    ASSERT_EQ(original_count + 1, dmGameObject::GetNumComponentTypes(fixture.m_Register));
    ASSERT_EQ(queue, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));

    dmGameObject::SortComponentTypes(fixture.m_Register);
    dmGameObject::HCollection collection = NewCollection(&fixture, "paired_registration");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));

    DeleteComponentRenderFixture(&fixture);
}

// Verify a render-enabled type cannot be added after world creation and leaves the original legacy path intact.
TEST(ComponentRender, LatePairedRegistrationLeavesTypesUnchanged)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false);
    const uint32_t original_count = dmGameObject::GetNumComponentTypes(fixture.m_Register);
    dmGameObject::ComponentType type = MakeGameType(&fixture, 1, "late_paired_component");
    dmGameSystem::ComponentRenderType render_type = MakeRenderType(&fixture.m_Probe);
    dmGameObject::HCollection collection = NewCollection(&fixture, "late_paired_registration");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION,
        dmGameSystem::RegisterComponentType(fixture.m_Context, type, &render_type));
    ASSERT_EQ(original_count, dmGameObject::GetNumComponentTypes(fixture.m_Register));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    DeleteComponentRenderFixture(&fixture);
}

// Verify finalization completes retained frame/world state without releasing borrowed services or component type storage.
TEST(ComponentRender, FinalizeCompletesRenderingBeforeContextStorage)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    dmGameObject::HCollection collection = NewCollection(&fixture, "finalize_rendering");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    dmGameSystem::ModelContext* model_context = &fixture.m_Context->m_ModelContext;
    model_context->m_Factory = fixture.m_Factory;
    model_context->m_MaxModelCount = 73;
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    DeleteCollection(&fixture, collection);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(1u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    // No separate drain or retirement call: Finalize finishes renderer lifetimes,
    // then releases the bridge while borrowed component contexts remain available.
    ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
    fixture.m_Render = 0;

    ASSERT_EQ((dmGameSystem::HComponentRenderContext)0, fixture.m_Context->m_ComponentRenderContext);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(model_context, &fixture.m_Context->m_ModelContext);
    ASSERT_EQ(fixture.m_Factory, model_context->m_Factory);
    ASSERT_EQ(73u, model_context->m_MaxModelCount);
    ASSERT_EQ(2u, dmGameObject::GetNumComponentTypes(fixture.m_Register));
    ASSERT_EQ(dmRender::EXECUTION_MODE_INLINE, dmRender::GetRenderExecutionMode(fixture.m_Execution));

    HResourceType resource_type;
    ASSERT_EQ(dmResource::RESULT_OK,
        dmResource::GetTypeFromExtension(fixture.m_Factory, "artificial_a", &resource_type));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteComponentRenderFixture(&fixture);
}

// Verify internal callback validation rejects partial, duplicate and late registration without losing valid registrations.
TEST(ComponentRender, RegistrationValidation)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2, false);
    dmGameSystem::ComponentRenderType invalid_type = {};
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameSystem::RegisterComponentRenderType(fixture.m_Render, 0, invalid_type));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, RegisterRenderer(&fixture, 100));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ(dmGameObject::RESULT_OK, RegisterRenderer(&fixture, 0));
    ASSERT_EQ(dmGameObject::RESULT_ALREADY_REGISTERED, RegisterRenderer(&fixture, 0));

    dmGameObject::HCollection collection = NewCollection(&fixture, "closed_render_registration");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, RegisterRenderer(&fixture, 1));
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    DeleteComponentRenderFixture(&fixture);
}

// Verify a rejected first registration releases its provisional queue and preserves the no-opt-in legacy path.
TEST(ComponentRender, FirstRejectedRegistrationReleasesProvisionalQueue)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false);
    dmGameObject::HCollection collection = NewCollection(&fixture, "late_first_render_registration");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, RegisterRenderer(&fixture, 0));
    ASSERT_EQ((dmRender::HRenderFrameQueue)0, dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));

    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    DeleteComponentRenderFixture(&fixture);
}

// Verify reserved producer credit is not an active capture scope and nested root calls cannot cancel the outer capture.
TEST(ComponentRender, GamesysOwnsCaptureScope)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false);
    dmGameSystem::ComponentRenderType type = MakeRenderType(&fixture.m_Probe);
    type.m_Capture = CaptureWithNestedAttempt;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::RegisterComponentRenderType(fixture.m_Render, 0, type));
    ArtificialType* game_type = (ArtificialType*)dmGameObject::GetComponentType(fixture.m_Register, 0)->m_Context;
    game_type->m_CaptureOwner = fixture.m_Render;
    // A migrated component need not register a legacy render callback at all.
    dmGameObject::GetComponentType(fixture.m_Register, 0)->m_RenderFunction = 0;

    dmGameObject::HCollection collection = NewCollection(&fixture, "gamesys_capture_scope");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    ASSERT_FALSE(dmGameObject::Render(collection));
    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    ASSERT_FALSE(dmGameObject::Render(collection));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));

    ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
    ASSERT_TRUE(game_type->m_NestedCaptureRejected);
    ASSERT_TRUE(game_type->m_NestedRenderRejected);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_FALSE(dmGameObject::Render(collection));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    DeleteComponentRenderFixture(&fixture);
}

// Verify association-table growth and collection handle reuse preserve distinct renderer lifetimes, also in inline-only builds.
TEST(ComponentRender, CollectionAssociationsSurviveGrowthAndReuse)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    dmGameObject::HCollection collections[10];
    for (uint32_t i = 0; i < 10; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "render_association_%u", i);
        collections[i] = NewCollection(&fixture, name);
        ASSERT_NE((dmGameObject::HCollection)0, collections[i]);
    }
    // Render only after growth so every association is looked up in the resized table.
    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collections[i]));
        dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
        ASSERT_EQ(GetWorld(collections[i])->m_Id, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, i).m_World);
    }

    dmGameObject::HCollection old_handle = collections[3];
    uint64_t old_world = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 3).m_RenderWorld;
    DeleteCollection(&fixture, old_handle);
    collections[3] = NewCollection(&fixture, "render_association_3");
    ASSERT_NE((dmGameObject::HCollection)0, collections[3]);
    ASSERT_NE(old_handle, collections[3]);
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collections[3]));
    ASSERT_NE(old_world, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 10).m_RenderWorld);
    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);

    dmGameObject::DeleteCollections(fixture.m_Register);
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmGameSystem::ServiceComponentRenderContext(fixture.m_Render));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_EQ(11u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
    fixture.m_Render = 0;
    ASSERT_TRUE(fixture.m_Register->m_ComponentWorldCallbacks.m_Context == 0);
    ASSERT_TRUE(fixture.m_Register->m_ComponentWorldCallbacks.m_OnWorldCreated == 0);
    ASSERT_TRUE(fixture.m_Register->m_ComponentWorldCallbacks.m_OnWorldDeleted == 0);
    ASSERT_TRUE(fixture.m_Register->m_ComponentWorldCallbacks.m_Render == 0);
    DeleteComponentRenderFixture(&fixture);
}

// Verify capture observes late-update/final transforms in sorted component order before PostUpdate mutates worlds.
TEST(ComponentRender, FinalTransformsAndOrderedInlineCapture)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2);
    dmGameObject::HCollection collection = NewCollection(&fixture, "ordered_render_callbacks");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    dmGameObject::HInstance parent = dmGameObject::New(collection, 0);
    dmGameObject::HInstance child = dmGameObject::New(collection, 0);
    ASSERT_NE((dmGameObject::HInstance)0, parent);
    ASSERT_NE((dmGameObject::HInstance)0, child);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child, parent));
    dmGameObject::SetPosition(parent, dmVMath::Point3(5.0f, 0.0f, 0.0f));
    GetWorld(collection)->m_Instance = child;
    ASSERT_TRUE(dmGameObject::Init(collection));

    dmGameObject::UpdateContext update;
    update.m_DT = 1.0f / 60.0f;
    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    ASSERT_TRUE(dmGameObject::Update(collection, &update));
    ASSERT_EQ(12.0f, dmGameObject::GetWorldPosition(child).getX());

    // PreRender can dirty transforms after Update has already resolved them.
    dmGameObject::SetPosition(parent, dmVMath::Point3(6.0f, 0.0f, 0.0f));
    dmGameObject::Delete(collection, child, false);

    ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, GetEvent(&fixture.m_Probe, EVENT_TYPE_CAPTURE, 0).m_ComponentIndex);
    ASSERT_EQ(0u, GetEvent(&fixture.m_Probe, EVENT_TYPE_CAPTURE, 1).m_ComponentIndex);
    ASSERT_EQ(1u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_ComponentIndex);
    ASSERT_EQ(0u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_ComponentIndex);
    ASSERT_EQ(13.0f, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_X);
    ASSERT_EQ(111u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_Value);
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_POST_UPDATE));

    ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    ASSERT_FALSE(dmGameObject::IsValid(child));
    ASSERT_EQ(1111u, GetWorld(collection)->m_Value);

    DeleteCollection(&fixture, collection);
    ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, 0));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify proxy-style legacy traversal inherits one capture frame, skips disabled/unrelated collections and refreshes child transforms.
TEST(ComponentRender, RecursiveRenderInheritsCaptureFrame)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2, false);
    ASSERT_EQ(dmGameObject::RESULT_OK, RegisterRenderer(&fixture, 0));

    dmGameObject::HCollection root = NewCollection(&fixture, "render_root");
    dmGameObject::HCollection child = NewCollection(&fixture, "render_child");
    dmGameObject::HCollection disabled = NewCollection(&fixture, "render_disabled");
    dmGameObject::HCollection unrelated = NewCollection(&fixture, "render_unrelated");
    ASSERT_NE((dmGameObject::HCollection)0, root);
    ASSERT_NE((dmGameObject::HCollection)0, child);
    ASSERT_NE((dmGameObject::HCollection)0, disabled);
    ASSERT_NE((dmGameObject::HCollection)0, unrelated);

    GetWorld(root, 1)->m_ChildCollection = child;
    GetWorld(root, 1)->m_ChildEnabled = true;
    GetWorld(child, 1)->m_ChildCollection = disabled;
    GetWorld(child, 1)->m_ChildEnabled = false;

    dmGameObject::HInstance instance = dmGameObject::New(child, 0);
    ASSERT_NE((dmGameObject::HInstance)0, instance);
    GetWorld(child)->m_Instance = instance;
    ASSERT_TRUE(dmGameObject::Init(child));

    dmGameObject::UpdateContext update;
    update.m_DT = 1.0f / 60.0f;
    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    ASSERT_TRUE(dmGameObject::Update(child, &update));

    dmGameObject::SetPosition(instance, dmVMath::Point3(9.0f, 0.0f, 0.0f));
    ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, root));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    ASSERT_EQ(GetWorld(child)->m_Id, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_World);
    ASSERT_EQ(GetWorld(root)->m_Id, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_World);
    ASSERT_EQ(9.0f, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_X);
    ASSERT_NE((dmRender::HRenderFrame)0, GetWorld(root)->m_LastCapturedFrame);
    ASSERT_EQ(GetWorld(root)->m_LastCapturedFrame, GetWorld(child)->m_LastCapturedFrame);
    ASSERT_EQ((dmRender::HRenderFrame)0, GetWorld(disabled)->m_LastCapturedFrame);
    ASSERT_EQ((dmRender::HRenderFrame)0, GetWorld(unrelated)->m_LastCapturedFrame);

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    dmGameObject::DeleteCollections(fixture.m_Register);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify cancellation of a partially captured component frame publishes no prefix and leaves no renderer state alive.
TEST(ComponentRender, CaptureFailureCancelsWholeFrame)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2);
    dmGameObject::HCollection collection = NewCollection(&fixture, "failed_render_capture");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    fixture.m_Types[0].m_FailCapture = true;
    ASSERT_FALSE(BeginAndRenderComponents(fixture.m_Context, collection));
    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);

    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));

    fixture.m_Types[0].m_FailCapture = false;
    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify a rejected collection attachment closes the new renderer lifetimes without affecting the existing collection.
TEST(ComponentRender, CollectionAttachFailureCleansUp)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    dmGameObject::HCollection collection = NewCollection(&fixture, "duplicate_render_collection");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_EQ((dmGameObject::HCollection)0, NewCollection(&fixture, "duplicate_render_collection"));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_CREATE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));

    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    ASSERT_EQ(1u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_World);

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    DeleteComponentRenderFixture(&fixture);
}

// Verify generic owner notification failure unwinds game worlds and gamesys associations using the original collection identity.
TEST(ComponentRender, WorldNotificationFailureRollsBack)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2, false);
    ASSERT_EQ(dmGameObject::RESULT_OK, RegisterRenderer(&fixture, 0));
    // The second registered slot stays empty when its creation notification fails.
    ASSERT_EQ(dmGameObject::RESULT_OK, RegisterRenderer(&fixture, 1));

    WorldObserverProbe probe = {};
    probe.m_Next = fixture.m_Register->m_ComponentWorldCallbacks;
    dmGameObject::ComponentWorldCallbacks callbacks = {};
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameObject::SetComponentWorldCallbacks(fixture.m_Register, callbacks));
    callbacks.m_Context = &probe;
    callbacks.m_OnWorldCreated = ObserveWorldCreated;
    callbacks.m_OnWorldDeleted = ObserveWorldDeleted;
    callbacks.m_Render = ObserveWorldRender;
    ASSERT_EQ(dmGameObject::RESULT_ALREADY_REGISTERED, dmGameObject::SetComponentWorldCallbacks(fixture.m_Register, callbacks));
    dmGameObject::ClearComponentWorldCallbacks(fixture.m_Register);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetComponentWorldCallbacks(fixture.m_Register, callbacks));

    ASSERT_EQ((dmGameObject::HCollection)0, NewCollection(&fixture, "failed_render_world"));
    ASSERT_NE((dmGameObject::HCollection)0, probe.m_CreatedCollection);
    ASSERT_EQ(probe.m_CreatedCollection, probe.m_DeletedCollection);
    ASSERT_EQ(2u, probe.m_DeletedCount);
    ASSERT_TRUE(dmGameObject::HasComponentWorldCreationStarted(fixture.m_Register));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, RegisterRenderer(&fixture, 1));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameObject::SetComponentWorldCallbacks(fixture.m_Register, callbacks));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_CREATE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));

    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));

    DeleteComponentRenderFixture(&fixture);
}

// Verify an accepted frame owns its inputs and renderer lifetime even after the live world is mutated and freed.
TEST(ComponentRender, AcceptedFrameSurvivesGameWorldDeletion)
{
    if (!HasThreads())
        return;

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 1);
    Executor executor;
    StartExecutor(&executor, &fixture);

    dmGameObject::HCollection collection = NewCollection(&fixture, "deleted_before_consume");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);

    dmRender::RenderFrameTicket ticket;
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &ticket));
    ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, collection, ticket.m_Frame));
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, ticket));

    GetWorld(collection)->m_Value = 999;
    DeleteCollection(&fixture, collection);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(1u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));

    ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_Value);
    ASSERT_EQ(1u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_World);
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));

    ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmGameSystem::GetComponentRenderResult(fixture.m_Render));

    dmRender::StopRenderFrameQueue(queue);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify a frame actively being read keeps its copied inputs and renderer state after the game world is destroyed.
TEST(ComponentRender, ReadingFrameSurvivesGameWorldDeletion)
{
    if (!HasThreads())
        return;

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 1);
    fixture.m_Probe.m_HoldConsume = true;
    Executor executor;
    StartExecutor(&executor, &fixture);

    dmGameObject::HCollection collection = NewCollection(&fixture, "deleted_during_consume");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);

    dmRender::RenderFrameTicket ticket;
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &ticket));
    ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, collection, ticket.m_Frame));
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, ticket));

    RequestPump(&executor, true);
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (!fixture.m_Probe.m_ConsumeEntered)
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    GetWorld(collection)->m_Value = 999;
    DeleteCollection(&fixture, collection);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmRender::RetireRenderFrames(queue));
    ASSERT_EQ(1u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseConsume = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, WaitPump(&executor));

    ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_Value);
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));

    ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    dmRender::StopRenderFrameQueue(queue);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify immediate same-name collection replacement creates a distinct renderer identity while the old frame remains accepted.
TEST(ComponentRender, SameNameReplacementKeepsDistinctRendererLifetimes)
{
    if (!HasThreads())
        return;

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 1);
    Executor executor;
    StartExecutor(&executor, &fixture);

    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);

    dmGameObject::HCollection first = NewCollection(&fixture, "replaced_render_collection");
    ASSERT_NE((dmGameObject::HCollection)0, first);

    dmRender::RenderFrameTicket old_frame, new_frame;
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &old_frame));
    ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, first, old_frame.m_Frame));
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, old_frame));

    dmGameObject::DeleteCollection(first); // NewCollection must remove this pending same-name collection.
    dmGameObject::HCollection replacement = NewCollection(&fixture, "replaced_render_collection");
    ASSERT_NE((dmGameObject::HCollection)0, replacement);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(2u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &new_frame));
    ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, replacement, new_frame.m_Frame));
    ASSERT_EQ(dmRender::RENDER_FRAME_WOULD_BLOCK, dmRender::PublishRenderFrame(queue, new_frame));

    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
    ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));

    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, new_frame));
    DeleteCollection(&fixture, replacement);

    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
    ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));

    Event old_draw = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0);
    Event new_draw = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1);

    ASSERT_EQ(10u, old_draw.m_Value);
    ASSERT_EQ(20u, new_draw.m_Value);
    ASSERT_NE(old_draw.m_World, new_draw.m_World);
    ASSERT_NE(old_draw.m_RenderWorld, new_draw.m_RenderWorld);
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    dmRender::StopRenderFrameQueue(queue);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify lazy initialization failure surfaces its error, destroys partial state once, and is not retried by later frames.
TEST(ComponentRender, RendererInitializationFailureIsNotRetried)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    fixture.m_Probe.m_FailRenderInit = true;

    dmGameObject::HCollection collection = NewCollection(&fixture, "failed_renderer_init");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_FALSE(BeginAndRenderComponents(fixture.m_Context, collection));
    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    ASSERT_FALSE(BeginAndRenderComponents(fixture.m_Context, collection));

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(dmRender::RENDER_FRAME_INVALID_ARGUMENT,
        dmGameSystem::GetComponentRenderResult(fixture.m_Render));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    DeleteComponentRenderFixture(&fixture);
}

// Verify a consume error is surfaced without skipping other accepted component entries or losing finalization.
TEST(ComponentRender, RendererConsumeErrorPreservesAcceptedEntries)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2);
    fixture.m_Probe.m_FailRenderConsume = true;

    dmGameObject::HCollection collection = NewCollection(&fixture, "failed_renderer_consume");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_FALSE(BeginAndRenderComponents(fixture.m_Context, collection));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_FALSE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_POST_UPDATE));

    DeleteCollection(&fixture, collection);
    ASSERT_FALSE(dmGameSystem::PostUpdate(fixture.m_Context, 0));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);

    DeleteComponentRenderFixture(&fixture);
}

// Verify inline consumption keeps frame references until the explicit surrounding-render retirement boundary.
TEST(ComponentRender, InlineRetirementIsExplicit)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    dmGameObject::HCollection collection = NewCollection(&fixture, "inline_retirement");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(1u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    DeleteComponentRenderFixture(&fixture);
}

// Verify a collection that never renders closes its lazy renderer lifetime without initialization or a destructor callback.
TEST(ComponentRender, NeverRenderedCollectionReleasesLifetime)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2);
    dmGameObject::HCollection collection = NewCollection(&fixture, "never_rendered");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    DeleteComponentRenderFixture(&fixture);
}

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
