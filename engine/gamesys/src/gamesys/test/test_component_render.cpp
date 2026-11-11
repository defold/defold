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
#include <dmsdk/dlib/intersection.h>
#include <gameobject/comp_anim.h>
#include <gameobject/component.h>
#include <gameobject/gameobject_private.h>
#include <gameobject/gameobject.h>
#include <graphics/null/graphics_null_private.h>
#include <platform/window.hpp>
#include <render/render_command.h>
#include <render/render_execution.h>
#include <render/render_frame_queue.h>
#include <render/render_frame.h>
#include <render/render_private.h>
#include <resource/resource.h>
#include <script/script.h>
#include <testmain/testmain.h>

#include "../../../../graphics/src/test/test_graphics_util.h"
#include "gamesys/component_render.h"
#include "gamesys/component_render_state_private.h"
#include "gamesys/gamesys.h"
#include "gamesys/gamesys_context_private.h"

namespace
{
    struct SnapshotRenderer;

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
        EVENT_TYPE_RENDER_CULL,
        EVENT_TYPE_RENDER_BATCH,
        EVENT_TYPE_RENDER_DRAW,
        EVENT_TYPE_RENDER_END_FRAME,
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
        dmRender::HRenderContext                m_RenderContext;
        SnapshotRenderer*                       m_SnapshotRenderer;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        uint32_t                                m_EventCount;
        uint32_t                                m_NextWorld;
        uint32_t                                m_TextFrameAtEnd;
        // Use full bytes, not bitfields: runtime flag writes must not read/modify/write
        // storage shared with concurrent, unlocked configuration reads.
        // Configuration stays immutable while the render executor is running.
        uint8_t                                 m_FailRenderInit;
        uint8_t                                 m_FailRenderConsume;
        uint8_t                                 m_HoldConsume;
        uint8_t                                 m_HoldDraw;
        // All concurrent accesses to these flags hold m_Mutex.
        uint8_t                                 m_WrongOwner;
        uint8_t                                 m_ConsumeEntered;
        uint8_t                                 m_ReleaseConsume;
        uint8_t                                 m_DrawEntered;
        uint8_t                                 m_ReleaseDraw;
    };

    struct ArtificialType
    {
        Probe*                                m_Probe;
        dmGameSystem::HComponentRenderContext m_CaptureOwner;
        uint32_t                              m_Index;
        uint8_t                               m_FailCapture : 1;
        uint8_t                               m_ObserveRetirement : 1;
        uint8_t                               m_DeleteCollectionOnPostUpdate : 1;
        uint8_t                               m_FailPostUpdate : 1;
        uint8_t                               m_NestedCaptureRejected : 1;
        uint8_t                               m_NestedRenderRejected : 1;
    };

    // Small game-side data set for the retained-item reference component.
    struct SnapshotItem
    {
        float    m_X;
        float    m_Radius;
        uint32_t m_Value;
        uint32_t m_Material;
        uint32_t m_Order;
        uint32_t m_BatchKey;
        float    m_Z;
        uint8_t  m_Enabled : 1;
        uint8_t  m_DepthOrder : 1;
        uint8_t  m_NoTypedBounds : 1;
    };

    struct ArtificialWorld
    {
        SnapshotItem                  m_Items[3];
        dmGameObject::HInstance       m_Instance;
        ArtificialType*               m_Type;
        dmRender::HRenderFrameStorage m_LastCapturedFrame;
        dmGameObject::HCollection     m_ChildCollection;
        uint32_t                      m_Id;
        uint32_t                      m_Value;
        uint32_t                      m_ItemCount;
        uint8_t                       m_ChildEnabled : 1;
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
        dmGameSystem::ComponentRenderFrame     m_Frame;
        Probe*                                m_Probe;
        dmRender::HRenderItem                  m_Item;
        dmRender::HRenderItemDispatch          m_Dispatch;
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
        dmRender::HRenderContext              m_RenderContext;
        dmGraphics::HContext                  m_Graphics;
        HWindow                               m_Window;
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

    static void WaitForDrawRelease(Probe* probe)
    {
        DM_MUTEX_SCOPED_LOCK(probe->m_Mutex);
        if (probe->m_HoldDraw)
        {
            probe->m_DrawEntered = true;
            dmConditionVariable::Broadcast(probe->m_Changed);
            while (!probe->m_ReleaseDraw)
            {
                dmConditionVariable::Wait(probe->m_Changed, probe->m_Mutex);
            }
        }
    }

    static void DrawArtificialRenderer(const dmRender::RenderListDispatchParams& params)
    {
        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            ArtificialRenderer* renderer = (ArtificialRenderer*)params.m_UserData;
            Probe* probe = renderer->m_Probe;
            WaitForDrawRelease(probe);

            // Dispatch runs after ConsumeRenderer has returned. Read the borrowed
            // payload here, rather than a value copied into the persistent item.
            assert(renderer->m_Frame.m_Frame);
            const ArtificialInput* input = (const ArtificialInput*)dmRender::GetRenderFrameTableData(
                *renderer->m_Frame.m_Frame, renderer->m_Frame.m_FirstTable);
            for (uint32_t* i = params.m_Begin; i != params.m_End; ++i)
            {
                const dmRender::RenderListEntry& entry = params.m_Buf[*i];
                RecordEvent(probe, EVENT_TYPE_RENDER_DRAW, input->m_Type, input->m_World, input->m_Value, input->m_X,
                            dmRender::EXECUTION_OWNER_RENDER, entry.m_UserData);
            }
        }
    }

    static dmRender::RenderFrameResult InitializeRenderer(void* context, const void* data, uint32_t size, void** state)
    {
        Probe* probe = (Probe*)context;
        assert(size == sizeof(dmGameSystem::ComponentRenderWorldInfo));
        ArtificialRenderer* renderer = (ArtificialRenderer*)calloc(1, sizeof(ArtificialRenderer));
        renderer->m_Info = *(const dmGameSystem::ComponentRenderWorldInfo*)data;
        renderer->m_Probe = probe;
        *state = renderer;
        RecordEvent(probe, EVENT_TYPE_RENDER_CREATE, renderer->m_Info.m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        if (probe->m_RenderContext)
        {
            renderer->m_Dispatch = dmRender::RegisterRenderListDispatch(probe->m_RenderContext, DrawArtificialRenderer, 0, renderer);
            renderer->m_Item = dmRender::NewRenderItem(probe->m_RenderContext, renderer->m_Dispatch, renderer->m_Info.m_WorldId);
            dmRender::SetMaterial(probe->m_RenderContext, renderer->m_Item, 0);
            dmRender::Enable(probe->m_RenderContext, renderer->m_Item, true);
        }
        return probe->m_FailRenderInit ? dmRender::RENDER_FRAME_INVALID_ARGUMENT : dmRender::RENDER_FRAME_OK;
    }

    static dmRender::RenderFrameResult ConsumeRenderer(void* context, void* state,
                                                       const dmGameSystem::ComponentRenderFrame& frame)
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
        assert(!renderer->m_Frame.m_Frame);
        renderer->m_Frame = frame;
        assert(frame.m_TableCount == 1);
        const ArtificialInput* input = (const ArtificialInput*)dmRender::GetRenderFrameTableData(*frame.m_Frame, frame.m_FirstTable);
        assert(frame.m_Frame->m_Tables[frame.m_FirstTable].m_Count == 1);
        assert(frame.m_Frame->m_Tables[frame.m_FirstTable].m_Stride == sizeof(*input));
        assert(input->m_Type == renderer->m_Info.m_ComponentIndex);

        RecordEvent((Probe*)context, EVENT_TYPE_RENDER_CONSUME, input->m_Type, input->m_World, input->m_Value, input->m_X, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        if (probe->m_RenderContext)
        {
            dmRender::SetBatchKey(probe->m_RenderContext, renderer->m_Item, input->m_Value);
            dmRender::SubmitRenderListDispatch(probe->m_RenderContext, renderer->m_Dispatch);
        }
        return ((Probe*)context)->m_FailRenderConsume ? dmRender::RENDER_FRAME_INVALID_ARGUMENT : dmRender::RENDER_FRAME_OK;
    }

    static void EndRendererFrame(void* context, void* state, const dmGameSystem::ComponentRenderFrame& frame)
    {
        ArtificialRenderer* renderer = (ArtificialRenderer*)state;
        assert(renderer->m_Frame.m_Frame == frame.m_Frame);
        assert(renderer->m_Frame.m_FirstTable == frame.m_FirstTable);
        assert(renderer->m_Frame.m_TableCount == frame.m_TableCount);
        const ArtificialInput* input = (const ArtificialInput*)dmRender::GetRenderFrameTableData(*frame.m_Frame, frame.m_FirstTable);
        RecordEvent((Probe*)context, EVENT_TYPE_RENDER_END_FRAME, input->m_Type, input->m_World, input->m_Value, input->m_X,
                    dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        if (renderer->m_Probe->m_RenderContext)
        {
            renderer->m_Probe->m_TextFrameAtEnd = renderer->m_Probe->m_RenderContext->m_TextContext.m_Frame;
        }
        memset(&renderer->m_Frame, 0, sizeof(renderer->m_Frame));
    }

    static void DeleteRenderer(void* context, void* state)
    {
        ArtificialRenderer* renderer = (ArtificialRenderer*)state;
        assert(!renderer->m_Frame.m_Frame);
        Probe* probe = (Probe*)context;
        if (probe->m_RenderContext)
        {
            dmRender::DeleteRenderItem(probe->m_RenderContext, renderer->m_Item);
            dmRender::UnregisterRenderListDispatch(probe->m_RenderContext, renderer->m_Dispatch);
        }
        RecordEvent((Probe*)context, EVENT_TYPE_RENDER_DELETE, renderer->m_Info.m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER, renderer->m_Info.m_WorldId);
        free(renderer);
    }

    static dmGameSystem::ComponentRenderType MakeRenderType(Probe* probe)
    {
        dmGameSystem::ComponentRenderType type;
        type.m_Capture = Capture;
        type.m_RenderCallbacks.m_Initialize = InitializeRenderer;
        type.m_RenderCallbacks.m_Consume = ConsumeRenderer;
        type.m_RenderCallbacks.m_EndFrame = EndRendererFrame;
        type.m_RenderCallbacks.m_Destroy = DeleteRenderer;
        type.m_RenderContext = probe;
        return type;
    }

    // Retained handles belong to the renderer. Only the borrowed descriptor changes
    // each frame; item user data is an index into that frame's immutable input array.
    struct SnapshotRenderer
    {
        dmRender::HRenderItem              m_Items[3];
        dmGameSystem::ComponentRenderFrame m_Frame;
        dmRender::HMaterial                m_Materials[2];
        Probe*                             m_Probe;
        dmGraphics::HProgram               m_Program;
        dmRender::HRenderItemDispatch      m_Dispatch;
        uint32_t                           m_ItemCount;
        uint32_t                           m_ComponentIndex;
    };

    static dmRender::RenderFrameResult CaptureSnapshotItems(const dmGameSystem::ComponentRenderCaptureParams& params)
    {
        ArtificialWorld* world = (ArtificialWorld*)params.m_World;
        // An empty capture still receives Consume/EndFrame, but needs no input table.
        if (!world->m_ItemCount)
        {
            return dmRender::RENDER_FRAME_OK;
        }
        uint32_t table;
        return dmRender::CopyRenderFrameTable(params.m_Frame, 0, world->m_ItemCount, sizeof(SnapshotItem),
                                              alignof(SnapshotItem), world->m_Items, &table);
    }

    static void DrawSnapshotItems(const dmRender::RenderListDispatchParams& params)
    {
        if (params.m_Operation != dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            return;
        }
        SnapshotRenderer* renderer = (SnapshotRenderer*)params.m_UserData;
        WaitForDrawRelease(renderer->m_Probe);
        ASSERT_NE((const dmRender::RenderFrameView*)0, renderer->m_Frame.m_Frame);
        const SnapshotItem* inputs = (const SnapshotItem*)dmRender::GetRenderFrameTableData(
            *renderer->m_Frame.m_Frame, renderer->m_Frame.m_FirstTable);
        RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_BATCH, renderer->m_ComponentIndex, 0,
                    (uint32_t)(params.m_End - params.m_Begin), 0, dmRender::EXECUTION_OWNER_RENDER);
        for (uint32_t* i = params.m_Begin; i != params.m_End; ++i)
        {
            const dmRender::RenderListEntry& entry = params.m_Buf[*i];
            const uint32_t index = (uint32_t)entry.m_UserData;
            const SnapshotItem& input = inputs[index];
            ASSERT_EQ(input.m_X, entry.m_WorldPosition.getX());
            ASSERT_EQ(input.m_Z, entry.m_WorldPosition.getZ());
            ASSERT_EQ(input.m_Order, entry.m_Order);
            ASSERT_EQ(input.m_BatchKey, entry.m_BatchKey);
            RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_DRAW, renderer->m_ComponentIndex, index, input.m_Value, input.m_X,
                        dmRender::EXECUTION_OWNER_RENDER, renderer->m_Items[index]);
        }
    }

    static void CullSnapshotItems(const dmRender::RenderListVisibilityParams& params)
    {
        SnapshotRenderer* renderer = (SnapshotRenderer*)params.m_UserData;
        const SnapshotItem* inputs = (const SnapshotItem*)dmRender::GetRenderFrameTableData(
            *renderer->m_Frame.m_Frame, renderer->m_Frame.m_FirstTable);
        RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_CULL, renderer->m_ComponentIndex, 0,
                    params.m_NumEntries, 0, dmRender::EXECUTION_OWNER_RENDER);
        for (uint32_t i = 0; i < params.m_NumEntries; ++i)
        {
            dmRender::RenderListEntry& entry = params.m_Entries[i];
            const SnapshotItem& input = inputs[entry.m_UserData];
            // Custom component culling reads only the current immutable snapshot.
            const dmVMath::Point3 center(input.m_X, 0, input.m_Z);
            entry.m_Visibility = dmIntersection::TestFrustumSphereSq(*params.m_Frustum, center, input.m_Radius * input.m_Radius)
                                ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    static dmRender::RenderFrameResult InitializeSnapshotRenderer(void* context, const void* data, uint32_t, void** state)
    {
        Probe* probe = (Probe*)context;
        SnapshotRenderer* renderer = (SnapshotRenderer*)calloc(1, sizeof(SnapshotRenderer));
        renderer->m_Probe = probe;
        renderer->m_ComponentIndex = ((const dmGameSystem::ComponentRenderWorldInfo*)data)->m_ComponentIndex;
        // The second test component supplies custom culling. Unbounded items in the
        // first component deliberately exercise the always-visible fallback.
        dmRender::RenderListVisibilityFn visibility = renderer->m_ComponentIndex == 1 ? CullSnapshotItems : 0;
        renderer->m_Dispatch = dmRender::RegisterRenderListDispatch(probe->m_RenderContext, DrawSnapshotItems, visibility, renderer);
        dmGraphics::ShaderDescBuilder shader;
        shader.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
        shader.AddShader(dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, "foo", 3);
        renderer->m_Program = dmGraphics::NewProgram(probe->m_RenderContext->m_GraphicsContext, shader.Get(), 0, 0);
        const dmhash_t tags[] = { dmHashString64("snapshot-blue"), dmHashString64("snapshot-red") };
        for (uint32_t i = 0; i < 2; ++i)
        {
            renderer->m_Materials[i] = dmRender::NewMaterial(probe->m_RenderContext, renderer->m_Program);
            dmRender::SetMaterialTags(renderer->m_Materials[i], 1, &tags[i]);
        }
        probe->m_SnapshotRenderer = renderer;
        *state = renderer;
        RecordEvent(probe, EVENT_TYPE_RENDER_CREATE, renderer->m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER);
        return dmRender::RENDER_FRAME_OK;
    }

    static dmRender::RenderFrameResult ConsumeSnapshotItems(void*, void* state, const dmGameSystem::ComponentRenderFrame& frame)
    {
        SnapshotRenderer* renderer = (SnapshotRenderer*)state;
        dmRender::HRenderContext context = renderer->m_Probe->m_RenderContext;
        renderer->m_Frame = frame;
        const uint32_t count = frame.m_TableCount ? frame.m_Frame->m_Tables[frame.m_FirstTable].m_Count : 0;
        const SnapshotItem* inputs = count ? (const SnapshotItem*)dmRender::GetRenderFrameTableData(*frame.m_Frame, frame.m_FirstTable) : 0;
        for (uint32_t i = renderer->m_ItemCount; i < count; ++i)
        {
            renderer->m_Items[i] = dmRender::NewRenderItem(context, renderer->m_Dispatch, i);
        }
        for (uint32_t i = count; i < renderer->m_ItemCount; ++i)
        {
            dmRender::Enable(context, renderer->m_Items[i], false);
        }
        if (renderer->m_ItemCount < count)
        {
            renderer->m_ItemCount = count;
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            const SnapshotItem& input = inputs[i];
            dmRender::HRenderItem item = renderer->m_Items[i];
            dmRender::SetWorldTransform(context, item, dmVMath::Matrix4::translation(dmVMath::Vector3(input.m_X, 0, input.m_Z)));
            if (input.m_NoTypedBounds)
            {
                dmRender::SetBoundingAABB(context, item, dmVMath::Vector3(1), dmVMath::Vector3(-1));
            }
            else
            {
                dmRender::SetBoundingSphere(context, item, dmVMath::Point3(input.m_X, 0, input.m_Z), input.m_Radius);
            }
            dmRender::SetMaterial(context, item, renderer->m_Materials[input.m_Material]);
            dmRender::SetOrder(context, item, input.m_DepthOrder ? dmRender::RENDER_ORDER_WORLD : dmRender::RENDER_ORDER_AFTER_WORLD, input.m_Order);
            dmRender::SetBatchKey(context, item, input.m_BatchKey);
            dmRender::Enable(context, item, input.m_Enabled);
        }
        dmRender::SubmitRenderListDispatch(context, renderer->m_Dispatch);
        RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_CONSUME, renderer->m_ComponentIndex, 0, count, 0, dmRender::EXECUTION_OWNER_RENDER);
        return dmRender::RENDER_FRAME_OK;
    }

    static void EndSnapshotFrame(void*, void* state, const dmGameSystem::ComponentRenderFrame&)
    {
        SnapshotRenderer* renderer = (SnapshotRenderer*)state;
        memset(&renderer->m_Frame, 0, sizeof(renderer->m_Frame));
        RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_END_FRAME, renderer->m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER);
    }

    static void DeleteSnapshotRenderer(void*, void* state)
    {
        SnapshotRenderer* renderer = (SnapshotRenderer*)state;
        dmRender::HRenderContext context = renderer->m_Probe->m_RenderContext;
        for (uint32_t i = 0; i < renderer->m_ItemCount; ++i)
        {
            dmRender::DeleteRenderItem(context, renderer->m_Items[i]);
        }
        dmRender::UnregisterRenderListDispatch(context, renderer->m_Dispatch);
        for (uint32_t i = 0; i < 2; ++i)
        {
            dmRender::DeleteMaterial(context, renderer->m_Materials[i]);
        }
        dmGraphics::DeleteProgram(context->m_GraphicsContext, renderer->m_Program);
        RecordEvent(renderer->m_Probe, EVENT_TYPE_RENDER_DELETE, renderer->m_ComponentIndex, 0, 0, 0, dmRender::EXECUTION_OWNER_RENDER);
        renderer->m_Probe->m_SnapshotRenderer = 0;
        free(renderer);
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

    static void NewComponentRenderFixture(ComponentRenderFixture* fixture, dmRender::ExecutionMode mode, uint32_t type_count, bool register_render = true, bool with_graphics = false)
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

        if (with_graphics)
        {
            dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
            WindowCreateParams window_params;
            WindowCreateParamsInitialize(&window_params);
            window_params.m_Width = 20;
            window_params.m_Height = 10;
            fixture->m_Window = dmPlatform::NewWindow();
            dmPlatform::OpenWindow(fixture->m_Window, window_params);

            dmGraphics::ContextParams graphics_params;
            graphics_params.m_Width = window_params.m_Width;
            graphics_params.m_Height = window_params.m_Height;
            graphics_params.m_Window = fixture->m_Window;
            fixture->m_Graphics = dmGraphics::NewContext(graphics_params);
        }
        dmScript::ContextParams script_params = {};
        script_params.m_GraphicsContext = fixture->m_Graphics;
        fixture->m_Script = dmScript::NewContext(script_params);
        dmScript::Initialize(fixture->m_Script);
        if (with_graphics)
        {
            dmRender::RenderContextParams render_params;
            render_params.m_ScriptContext = fixture->m_Script;
            render_params.m_MaxInstances = 8;
            render_params.m_MaxCharacters = 8;
            render_params.m_MaxBatches = 1;
            fixture->m_RenderContext = dmRender::NewRenderContext(fixture->m_Graphics, render_params);
            fixture->m_Probe.m_RenderContext = fixture->m_RenderContext;
        }
        fixture->m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(fixture->m_Register, fixture->m_Script);

        dmGameSystem::ContextParams context_params = {};
        context_params.m_Factory = fixture->m_Factory;
        context_params.m_Register = fixture->m_Register;
        context_params.m_RenderContext = fixture->m_RenderContext;
        context_params.m_ScriptContext = fixture->m_Script;
        const dmRender::RenderFrameLimits limits = { 4096, 16, 16 };
        fixture->m_Context = dmGameSystem::NewContext(context_params, fixture->m_Execution, limits, dmRender::RENDER_FRAME_PREPARATION_CONSUMER);
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
        if (fixture->m_RenderContext)
        {
            dmRender::DeleteRenderContext(fixture->m_RenderContext, fixture->m_Script);
        }
        dmScript::Finalize(fixture->m_Script);
        dmScript::DeleteContext(fixture->m_Script);
        dmGameObject::DeleteRegister(fixture->m_Register);
        dmResource::DeleteFactory(fixture->m_Factory);
        dmGameSystem::DeleteContext(fixture->m_Context);
        dmRender::DeleteRenderExecution(fixture->m_Execution);
        dmConditionVariable::Delete(fixture->m_Probe.m_Changed);
        dmMutex::Delete(fixture->m_Probe.m_Mutex);
        if (fixture->m_Graphics)
        {
            dmGraphics::DeleteContext(fixture->m_Graphics);
            dmPlatform::CloseWindow(fixture->m_Window);
            dmPlatform::DeleteWindow(fixture->m_Window);
        }
    }

    // Tests without a simulation step still reserve before capture and submit only
    // after recording. Keep PostUpdate separate to inspect inputs before retirement.
    static bool BeginAndRenderComponents(dmGameSystem::HContext context, dmGameObject::HCollection collection)
    {
        return dmGameSystem::BeginFrame(context) && dmGameSystem::Render(context, collection)
               && dmGameSystem::SubmitRender(context, 0, 0.0f, 0);
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

    // Each snapshot moves both components and their cameras, and reverses draw order
    // on alternate frames. Neither live item data nor camera handles survive capture.
    static void RecordOverlapFrame(ComponentRenderFixture* fixture, dmGameObject::HCollection collection,
                                   uint32_t frame_index, dmRender::RenderFrameTicket* ticket)
    {
        const float origin = (float)frame_index * 10.0f;
        for (uint32_t component = 0; component < 2; ++component)
        {
            ArtificialWorld* world = GetWorld(collection, component);
            world->m_ItemCount = 2;
            for (uint32_t item = 0; item < 2; ++item)
            {
                SnapshotItem& input = world->m_Items[item];
                memset(&input, 0, sizeof(input));
                input.m_X = origin + (float)item * 4.0f + (component ? 0.25f : -0.5f);
                input.m_Radius = 0.1f;
                input.m_Value = 1000 * frame_index + 10 * (component + 1) + item;
                input.m_Order = (frame_index % 2 ? 1 - component : component) + 1;
                input.m_BatchKey = 77;
                input.m_Enabled = true;
                input.m_NoTypedBounds = component == 1;
            }
        }

        dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture->m_Render);
        ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, ticket));
        dmGameSystem::BeginRender(fixture->m_Context, (float)frame_index + 1, 0.125f);
        ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture->m_Render, collection, ticket->m_Frame));
        ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderFrameInfo(ticket->m_Frame,
                                                                             dmRender::GetRenderFrameInfo(fixture->m_RenderContext)));

        dmRender::HRenderCamera camera = dmRender::NewRenderCamera(fixture->m_RenderContext);
        dmRender::RenderCamera* camera_data = fixture->m_RenderContext->m_Recording.m_RenderCameras.Get(camera);
        dmRender::Predicate predicate = {};
        dmRender::Command commands[] = {
            dmRender::Command(dmRender::COMMAND_TYPE_DRAW), dmRender::Command(dmRender::COMMAND_TYPE_DRAW),
            dmRender::Command(dmRender::COMMAND_TYPE_DRAW)
        };
        for (uint32_t pass = 0; pass < 3; ++pass)
        {
            const float view_origin = origin + (pass == 1 ? 4.0f : 0.0f);
            camera_data->m_View = dmVMath::Matrix4::translation(dmVMath::Vector3(-view_origin, 0, 0));
            camera_data->m_Projection = dmVMath::Matrix4::identity();
            camera_data->m_ViewProjection = camera_data->m_View;
            camera_data->m_Dirty = 0;
            dmRender::Command source(dmRender::COMMAND_TYPE_DRAW, (uint64_t)&predicate, 0, 0, dmRender::SORT_BACK_TO_FRONT);
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderCommand(fixture->m_RenderContext,
                                                                               ticket->m_Frame, source, &commands[pass], camera, true, true));
        }
        ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::AppendRenderCommands(ticket->m_Frame, commands, 3));
        dmRender::DeleteRenderCamera(fixture->m_RenderContext, camera);

        for (uint32_t component = 0; component < 2; ++component)
        {
            ArtificialWorld* world = GetWorld(collection, component);
            memset(world->m_Items, 0xdd, sizeof(world->m_Items));
            world->m_ItemCount = 0;
        }
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
            ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));
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

// Verify component capture remains unpublished until explicit submission, preserving the captured values in either mode.
TEST(ComponentRender, CaptureWaitsForCompleteFrameSubmission)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1);
        fixture.m_Types[0].m_ObserveRetirement = true;
        Executor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartExecutor(&executor, &fixture);
        }

        dmGameObject::HCollection collection = NewCollection(&fixture, "capture_before_submission");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));

        dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
        dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(queue);
        ASSERT_EQ((uint64_t)0, status.m_Published);
        ASSERT_EQ((uint64_t)0, status.m_Consumed);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, true));
        }

        // Recording later frame commands may mutate game state, but must not change
        // the component input already captured for this frame.
        GetWorld(collection)->m_Value = 999;
        ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
        }

        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_Value);
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));

        DeleteCollection(&fixture, collection);
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
            dmRender::StopRenderFrameQueue(queue);
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
        }
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify abandoning a captured but unsubmitted frame releases its references without initializing a renderer.
TEST(ComponentRender, PostUpdateCancelsUnsubmittedCapture)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1);
        fixture.m_Types[0].m_ObserveRetirement = true;
        AutomaticExecutor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartAutomaticExecutor(&executor, &fixture);
        }

        dmGameObject::HCollection collection = NewCollection(&fixture, "cancel_captured_frame");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));

        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));

        dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
        ASSERT_EQ((uint64_t)0, status.m_Published);
        ASSERT_EQ((uint64_t)1, status.m_Cancelled);
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[0]);
        ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[1]);

        // The cancelled frame must not poison the next frame or retain its world.
        ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
        DeleteCollection(&fixture, collection);
        ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
        fixture.m_Render = 0;

        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(1010u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_Value);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify a later recording failure cancels already captured component inputs instead of publishing a partial frame.
TEST(ComponentRender, RecordingFailureCancelsCapturedComponents)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1);
        fixture.m_Types[0].m_ObserveRetirement = true;
        AutomaticExecutor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartAutomaticExecutor(&executor, &fixture);
        }

        dmGameObject::HCollection collection = NewCollection(&fixture, "failed_frame_recording");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));

        // A constant-like snapshot may already own dynamic storage when a later fixed
        // payload fails. Neither the snapshot nor the component inputs may be published.
        dmRender::HRenderFrameStorage frame = GetWorld(collection)->m_LastCapturedFrame;
        uint32_t table_index;
        void* payload;
        ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::AllocateRenderFrameDynamicTable(frame, 0, 1, 128 * 1024, 16, &table_index, &payload));
        memset(payload, 0x5a, 128 * 1024);
        const uint64_t dynamic_capacity = dmRender::GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity;
        ASSERT_EQ(dmRender::RENDER_FRAME_CAPACITY_EXCEEDED, dmRender::AllocateRenderFrameTable(frame, 0, 1, 4096, 16, &table_index, &payload));
        ASSERT_FALSE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));
        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        ASSERT_EQ((uint64_t)0, dmRender::GetRenderFrameStatistics(frame).m_DynamicPayloadBytes);
        ASSERT_EQ(dynamic_capacity, dmRender::GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));

        dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
        ASSERT_EQ((uint64_t)0, status.m_Published);
        ASSERT_EQ((uint64_t)1, status.m_Cancelled);

        ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
        DeleteCollection(&fixture, collection);
        ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
        fixture.m_Render = 0;

        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
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

// Graphics-free lifecycle users need no queue until a component opts into owned inputs.
TEST(ComponentRender, GraphicsFreeContextWithoutRenderTypesAllocatesNoFrames)
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

// Verify legacy component callbacks remain inline-only until they stop exposing mutable game-world data to rendering.
TEST(ComponentRender, LegacyCallbacksRequireInlineExecution)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 2, false);
        ASSERT_EQ(dmGameObject::RESULT_OK, RegisterRenderer(&fixture, 0));
        AutomaticExecutor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartAutomaticExecutor(&executor, &fixture);
        }

        dmGameObject::HCollection collection = NewCollection(&fixture, "legacy_render_admission");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        if (modes[i] == dmRender::EXECUTION_MODE_INLINE)
        {
            ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
            ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));
            ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
            ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        }
        else
        {
            ASSERT_FALSE(dmGameSystem::Render(fixture.m_Context, collection));
            ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
            ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
            dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
            ASSERT_EQ((uint64_t)0, status.m_Published);
        }

        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        DeleteCollection(&fixture, collection);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify consumer-owned preparation rejects legacy submissions instead of clearing them before drawing.
TEST(ComponentRender, ConsumerPreparationRejectsLegacyRenderCallbacks)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1, false, true);
    dmGameObject::HCollection collection = NewCollection(&fixture, "legacy_consumer_preparation");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    ASSERT_FALSE(dmGameSystem::Render(fixture.m_Context, collection));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    ASSERT_EQ((uint64_t)0, dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render)).m_Published);

    DeleteCollection(&fixture, collection);
    DeleteComponentRenderFixture(&fixture);
}

// Verify threaded setup cannot preserve a list that an inline producer would prepare.
TEST(ComponentRender, LegacyPreparationRejectsThreadedSetup)
{
    if (!HasThreads())
    {
        return;
    }

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 0, false, true);
    ASSERT_TRUE(dmGameSystem::Finalize(fixture.m_Context));
    fixture.m_Render = 0;
    dmRender::ExecutionSelection selection;
    dmRender::HRenderExecution execution = dmRender::NewRenderExecution(dmRender::EXECUTION_MODE_THREADED, true, &selection);
    ASSERT_NE((dmRender::HRenderExecution)0, execution);
    const dmRender::RenderFrameLimits limits = { 4096, 16, 16 };

    // Finalization removed the fixture's bridge: rejection must come from the
    // preparation/execution combination, not conflicting lifecycle hooks.
    ASSERT_EQ((dmGameSystem::HComponentRenderContext)0,
              dmGameSystem::NewComponentRenderContext(fixture.m_Register, execution, limits, fixture.m_RenderContext, dmRender::RENDER_FRAME_PREPARATION_LEGACY));

    dmRender::DeleteRenderExecution(execution);
    DeleteComponentRenderFixture(&fixture);
}

// Verify snapshots alone update retained items, including tags, bounds, order, disabled tails and omitted worlds.
TEST(ComponentRender, RetainedItemsFollowPublishedSnapshots)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t mode = 0; mode < mode_count; ++mode)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[mode], 1, false, true);
        dmGameSystem::ComponentRenderType type;
        type.m_Capture = CaptureSnapshotItems;
        type.m_RenderCallbacks.m_Initialize = InitializeSnapshotRenderer;
        type.m_RenderCallbacks.m_Consume = ConsumeSnapshotItems;
        type.m_RenderCallbacks.m_EndFrame = EndSnapshotFrame;
        type.m_RenderCallbacks.m_Destroy = DeleteSnapshotRenderer;
        type.m_RenderContext = &fixture.m_Probe;
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::RegisterComponentRenderType(fixture.m_Render, 0, type));
        dmGameObject::HCollection collection = NewCollection(&fixture, "snapshot_render_items");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        Executor executor;
        if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartExecutor(&executor, &fixture);
        }

        dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
        dmRender::HRenderItem retained_items[3];
        const uint32_t item_counts[] = { 3, 3, 1, 0, 2, 3, 1 };
        const uint32_t draw_counts[] = { 1, 2, 1, 0, 1, 0, 1 };
        const uint32_t draw_indices[][2] = { { 0, 0 }, { 2, 1 }, { 0, 0 }, { 0, 0 }, { 1, 0 }, { 0, 0 }, { 0, 0 } };
        for (uint32_t frame_index = 0; frame_index < 7; ++frame_index)
        {
            SnapshotItem inputs[] = {
                { 0.0f, 0.1f, 10 + frame_index, 0, 30, 100 + frame_index, 0.0f, 1, 0, 0 },
                { 0.25f, 0.1f, 20 + frame_index, 1, 20, 200 + frame_index, 0.0f, 1, 0, 0 },
                { 100.0f, 0.1f, 30 + frame_index, 0, 10, 300 + frame_index, 0.0f, 1, 0, 0 }
            };
            if (frame_index == 1)
            {
                // The same frustum must see changed bounds and the red-to-blue
                // material change. Explicit order puts item 2 before item 1.
                inputs[0].m_X = 100.0f;
                inputs[1].m_Material = 0;
                inputs[2].m_X = 0.5f;
            }
            if (frame_index == 4)
            {
                inputs[0].m_Enabled = false;
                inputs[1].m_Material = 0;
            }
            ArtificialWorld* world = GetWorld(collection);
            memcpy(world->m_Items, inputs, sizeof(inputs));
            world->m_ItemCount = item_counts[frame_index];
            const uint32_t first_draw = CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW);
            dmRender::RenderFrameTicket ticket;
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &ticket));
            dmGameSystem::BeginRender(fixture.m_Context, (float)frame_index, 0.125f);
            if (frame_index != 5)
            {
                ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, collection, ticket.m_Frame));
            }
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderFrameInfo(ticket.m_Frame,
                                                                                 dmRender::GetRenderFrameInfo(fixture.m_RenderContext)));
            dmRender::Predicate predicate = {};
            predicate.m_TagCount = 1;
            predicate.m_Tags[0] = dmHashString64("snapshot-blue");
            dmRender::FrustumOptions frustum;
            frustum.m_Matrix = dmVMath::Matrix4::identity();
            frustum.m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
            dmRender::Command commands[] = { dmRender::Command(dmRender::COMMAND_TYPE_DRAW), dmRender::Command(dmRender::COMMAND_TYPE_DRAW) };
            for (uint32_t pass = 0; pass < 2; ++pass)
            {
                dmRender::Command source(dmRender::COMMAND_TYPE_DRAW, (uint64_t)&predicate, 0, (uint64_t)&frustum, dmRender::SORT_BACK_TO_FRONT);
                ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderCommand(fixture.m_RenderContext,
                                                                                   ticket.m_Frame, source, &commands[pass], 0, false, false));
            }
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::AppendRenderCommands(ticket.m_Frame, commands, 2));

            // Only ConsumeSnapshotItems may modify retained items; neither capture
            // nor later game mutations can install render state or borrowed pointers.
            ASSERT_EQ(first_draw, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
            if (frame_index == 0)
            {
                ASSERT_EQ((SnapshotRenderer*)0, fixture.m_Probe.m_SnapshotRenderer);
            }
            memset(world->m_Items, 0xdd, sizeof(world->m_Items));
            world->m_ItemCount = 0;
            if (modes[mode] == dmRender::EXECUTION_MODE_INLINE)
            {
                dmGraphics::BeginFrame(fixture.m_Graphics);
            }
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, ticket));
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
            }

            SnapshotRenderer* renderer = fixture.m_Probe.m_SnapshotRenderer;
            ASSERT_EQ(3u, renderer->m_ItemCount);
            ASSERT_EQ((const dmRender::RenderFrameView*)0, renderer->m_Frame.m_Frame);
            ASSERT_EQ(first_draw + 2 * draw_counts[frame_index], CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
            ASSERT_EQ(frame_index == 5 ? 0u : 1u, fixture.m_RenderContext->m_SubmittedRenderItemDispatches.Size());
            for (uint32_t draw = 0; draw < 2 * draw_counts[frame_index]; ++draw)
            {
                const uint32_t index = draw_indices[frame_index][draw % draw_counts[frame_index]];
                Event event = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, first_draw + draw);
                ASSERT_EQ(index, event.m_World);
                ASSERT_EQ(inputs[index].m_Value, event.m_Value);
                ASSERT_EQ(inputs[index].m_X, event.m_X);
                ASSERT_EQ(renderer->m_Items[index], event.m_RenderWorld);
            }
            for (uint32_t item = 0; item < 3; ++item)
            {
                if (frame_index == 0)
                {
                    retained_items[item] = renderer->m_Items[item];
                }
                ASSERT_EQ(retained_items[item], renderer->m_Items[item]);
                if (frame_index != 5)
                {
                    const bool enabled = item < item_counts[frame_index] && inputs[item].m_Enabled;
                    ASSERT_EQ(enabled, fixture.m_RenderContext->m_PersistentRenderItems[(uint32_t)retained_items[item]].m_Enabled != 0);
                }
            }
            ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
        }

        DeleteCollection(&fixture, collection);
        if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
            dmRender::StopRenderFrameQueue(queue);
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
        }
        else
        {
            dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
        }
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify each recorded view culls and globally orders mixed snapshot components, batching only adjacent compatible runs.
TEST(ComponentRender, RecordedViewsPrepareMixedSnapshotItems)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t mode = 0; mode < mode_count; ++mode)
    {
        for (uint32_t depth_order = 0; depth_order < 2; ++depth_order)
        {
            ComponentRenderFixture fixture;
            NewComponentRenderFixture(&fixture, modes[mode], 2, false, true);
            dmGameSystem::ComponentRenderType type;
            type.m_Capture = CaptureSnapshotItems;
            type.m_RenderCallbacks.m_Initialize = InitializeSnapshotRenderer;
            type.m_RenderCallbacks.m_Consume = ConsumeSnapshotItems;
            type.m_RenderCallbacks.m_EndFrame = EndSnapshotFrame;
            type.m_RenderCallbacks.m_Destroy = DeleteSnapshotRenderer;
            type.m_RenderContext = &fixture.m_Probe;
            for (uint32_t component = 0; component < 2; ++component)
            {
                ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::RegisterComponentRenderType(fixture.m_Render, component, type));
            }
            dmGameObject::HCollection collection = NewCollection(&fixture, "mixed_snapshot_views");
            ASSERT_NE((dmGameObject::HCollection)0, collection);
            Executor executor;
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                StartExecutor(&executor, &fixture);
            }

            // Matching batch keys must not regroup component 0 around component 1.
            // Items 20/21 are adjacent and compatible; all other runs are separate.
            SnapshotItem inputs[2][3] = {
                {
                    { -0.5f, 0.1f, 10, 0, 10, 77,  0.6f, 1, 0, 0 },
                    {  3.5f, 0.1f, 11, 0, 40, 77,  0.0f, 1, 0, 0 },
                    { 100.0f, 0.1f, 12, 0, 60, 77, -0.4f, 1, 0, 1 }
                },
                {
                    { -0.25f, 0.1f, 20, 0, 20, 77,  0.4f, 1, 0, 1 },
                    {  0.25f, 0.1f, 21, 0, 30, 77,  0.2f, 1, 0, 1 },
                    {  3.75f, 0.1f, 22, 0, 50, 77, -0.2f, 1, 0, 1 }
                }
            };
            for (uint32_t component = 0; component < 2; ++component)
            {
                for (uint32_t item = 0; item < 3; ++item)
                {
                    inputs[component][item].m_DepthOrder = depth_order;
                }
                ArtificialWorld* world = GetWorld(collection, component);
                memcpy(world->m_Items, inputs[component], sizeof(world->m_Items));
                world->m_ItemCount = 3;
            }

            dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
            dmRender::RenderFrameTicket ticket;
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &ticket));
            dmGameSystem::BeginRender(fixture.m_Context, 1.0f, 0.125f);
            ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, collection, ticket.m_Frame));
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderFrameInfo(ticket.m_Frame,
                                                                                 dmRender::GetRenderFrameInfo(fixture.m_RenderContext)));

            dmRender::Predicate predicate = {};
            dmRender::FrustumOptions views[2];
            views[0].m_Matrix = dmVMath::Matrix4::identity();
            views[0].m_NumPlanes = dmRender::FRUSTUM_PLANES_ALL;
            views[1] = views[0];
            views[1].m_Matrix = dmVMath::Matrix4::translation(dmVMath::Vector3(-4, 0, 0));
            const dmRender::FrustumOptions* pass_views[] = { &views[0], &views[1], &views[0], 0 };
            dmRender::Command commands[] = {
                dmRender::Command(dmRender::COMMAND_TYPE_DRAW), dmRender::Command(dmRender::COMMAND_TYPE_DRAW),
                dmRender::Command(dmRender::COMMAND_TYPE_DRAW), dmRender::Command(dmRender::COMMAND_TYPE_DRAW)
            };
            for (uint32_t pass = 0; pass < 4; ++pass)
            {
                dmRender::Command source(dmRender::COMMAND_TYPE_DRAW, (uint64_t)&predicate, 0,
                                         (uint64_t)pass_views[pass], dmRender::SORT_BACK_TO_FRONT);
                ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderCommand(fixture.m_RenderContext,
                                                                                   ticket.m_Frame, source, &commands[pass], 0, false, false));
            }
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::AppendRenderCommands(ticket.m_Frame, commands, 4));

            // Both custom culling and batch callbacks must use the captured data.
            // Recorded views must likewise survive mutation before consumption.
            for (uint32_t component = 0; component < 2; ++component)
            {
                ArtificialWorld* world = GetWorld(collection, component);
                memset(world->m_Items, 0xdd, sizeof(world->m_Items));
                world->m_ItemCount = 0;
            }
            memset(views, 0xdd, sizeof(views));
            if (modes[mode] == dmRender::EXECUTION_MODE_INLINE)
            {
                dmGraphics::BeginFrame(fixture.m_Graphics);
            }
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, ticket));
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
            }

            // A/B/A must recompute visibility for each view. No-frustum draws restore
            // every item, including those hidden in A; item 12 is always visible.
            const uint32_t draw_values[] = {
                10, 20, 21, 12,
                11, 22, 12,
                10, 20, 21, 12,
                10, 20, 21, 11, 22, 12
            };
            ASSERT_EQ((uint32_t)DM_ARRAY_SIZE(draw_values), CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(draw_values); ++i)
            {
                Event event = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, i);
                ASSERT_EQ(draw_values[i], event.m_Value);
                ASSERT_EQ(draw_values[i] / 10 - 1, event.m_ComponentIndex);
                ASSERT_EQ(draw_values[i] % 10, event.m_World);
            }

            const uint32_t batch_components[] = { 0, 1, 0,  0, 1, 0,  0, 1, 0,  0, 1, 0, 1, 0 };
            const uint32_t batch_sizes[] =      { 1, 2, 1,  1, 1, 1,  1, 2, 1,  1, 2, 1, 1, 1 };
            ASSERT_EQ((uint32_t)DM_ARRAY_SIZE(batch_sizes), CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_BATCH));
            for (uint32_t i = 0; i < DM_ARRAY_SIZE(batch_sizes); ++i)
            {
                Event event = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_BATCH, i);
                ASSERT_EQ(batch_components[i], event.m_ComponentIndex);
                ASSERT_EQ(batch_sizes[i], event.m_Value);
            }
            ASSERT_EQ(3u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CULL));
            for (uint32_t i = 0; i < 3; ++i)
            {
                Event event = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CULL, i);
                ASSERT_EQ(1u, event.m_ComponentIndex);
                ASSERT_EQ(3u, event.m_Value);
            }
            ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
            ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));

            DeleteCollection(&fixture, collection);
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
                dmRender::StopRenderFrameQueue(queue);
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
            }
            else
            {
                dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
            }
            ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
            ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
            DeleteComponentRenderFixture(&fixture);
        }
    }
}

// Verify BeginRender supplies recording timing and both consumers populate render items before pass commands draw them.
TEST(ComponentRender, ComponentInputsAreConsumedBeforePassCommands)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t i = 0; i < mode_count; ++i)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[i], 1, true, true);
        Executor executor;
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartExecutor(&executor, &fixture);
        }
        dmGameObject::HCollection collection = NewCollection(&fixture, "complete_frame_order");
        ASSERT_NE((dmGameObject::HCollection)0, collection);

        // Only record timing here; the consumer prepares renderer state before
        // populating items from this snapshot.
        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        dmGameSystem::BeginRender(fixture.m_Context, 1.0f, 0.125f);
        ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));

        GetWorld(collection)->m_Value = 999;
        const uint32_t first_consume_event = fixture.m_Probe.m_EventCount;
        dmGraphics::NullContext* graphics = (dmGraphics::NullContext*)fixture.m_Graphics;
        if (modes[i] == dmRender::EXECUTION_MODE_INLINE)
        {
            // The inline engine begins graphics before recording Lua/pass commands.
            dmGraphics::BeginFrame(fixture.m_Graphics);
        }
        ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.125f, 0));
        dmGameSystem::BeginRender(fixture.m_Context, 99.0f, 0.5f);
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(0u, graphics->m_BeginFrameCount);
            ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
        }

        ASSERT_EQ(1u, graphics->m_BeginFrameCount);
        dmGraphics::Flip(fixture.m_Graphics);
        ASSERT_EQ(1u, graphics->m_FlipCount);
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
        ASSERT_EQ(EVENT_TYPE_RENDER_CREATE, fixture.m_Probe.m_Events[first_consume_event].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_CONSUME, fixture.m_Probe.m_Events[first_consume_event + 1].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_DRAW, fixture.m_Probe.m_Events[first_consume_event + 2].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_END_FRAME, fixture.m_Probe.m_Events[first_consume_event + 3].m_EventType);
        ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, 0).m_Value);
        ASSERT_EQ(1.0f, fixture.m_RenderContext->m_Time);
        ASSERT_EQ(0.125f, fixture.m_RenderContext->m_Dt);
        ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));

        DeleteCollection(&fixture, collection);
        if (modes[i] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
            dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
            dmRender::StopRenderFrameQueue(queue);
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
        }
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify borrowed inputs last through every pass, end without any matching draw, and clear before storage reuse.
TEST(ComponentRender, FrameBindingsSpanAllPasses)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t mode = 0; mode < mode_count; ++mode)
    {
        // No commands, a filtered-out draw, and two actual draws all share the same
        // complete-frame end boundary, regardless of render-list END callbacks.
        for (uint32_t pass_count = 0; pass_count <= 2; ++pass_count)
        {
            ComponentRenderFixture fixture;
            NewComponentRenderFixture(&fixture, modes[mode], 1, true, true);
            fixture.m_Types[0].m_ObserveRetirement = true;
            Executor executor;
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                StartExecutor(&executor, &fixture);
            }

            dmGameObject::HCollection collection = NewCollection(&fixture, "frame_scoped_bindings");
            ASSERT_NE((dmGameObject::HCollection)0, collection);
            dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
            const uint32_t draws_per_frame = pass_count == 2 ? 2 : 0;
            const uint32_t initial_text_frame = fixture.m_RenderContext->m_TextContext.m_Frame;
            for (uint32_t frame_index = 0; frame_index < 2; ++frame_index)
            {
                const uint32_t captured_value = 10 + 100 * frame_index;
                GetWorld(collection)->m_Value = captured_value;
                dmRender::RenderFrameTicket ticket;
                ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::BeginRenderFrameCapture(queue, &ticket));
                dmGameSystem::BeginRender(fixture.m_Context, (float)frame_index, 0.125f);
                ASSERT_TRUE(dmGameSystem::CaptureComponentRenderData(fixture.m_Render, collection, ticket.m_Frame));
                ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderFrameInfo(ticket.m_Frame,
                                                                                     dmRender::GetRenderFrameInfo(fixture.m_RenderContext)));

                dmRender::Predicate predicate = {};
                predicate.m_TagCount = 1;
                predicate.m_Tags[0] = dmHashString64("not-an-artificial-item");
                dmRender::Command commands[] = { dmRender::Command(dmRender::COMMAND_TYPE_DRAW), dmRender::Command(dmRender::COMMAND_TYPE_DRAW) };
                for (uint32_t pass = 0; pass < pass_count; ++pass)
                {
                    dmRender::Command source(dmRender::COMMAND_TYPE_DRAW, pass_count == 1 ? (uint64_t)&predicate : 0, 0, 0, dmRender::SORT_NONE);
                    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::RecordRenderCommand(fixture.m_RenderContext,
                                                                                       ticket.m_Frame, source, &commands[pass], 0, false, false));
                }
                if (pass_count)
                {
                    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::AppendRenderCommands(ticket.m_Frame, commands, pass_count));
                }

                GetWorld(collection)->m_Value = 999;
                if (modes[mode] == dmRender::EXECUTION_MODE_INLINE)
                {
                    dmGraphics::BeginFrame(fixture.m_Graphics);
                }
                ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, ticket));
                if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
                {
                    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
                }

                ASSERT_EQ(frame_index + 1, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
                ASSERT_EQ((frame_index + 1) * draws_per_frame, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
                ASSERT_EQ(frame_index + 1, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
                ASSERT_EQ(captured_value, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME, frame_index).m_Value);
                ASSERT_EQ(initial_text_frame + frame_index, fixture.m_Probe.m_TextFrameAtEnd);
                ASSERT_EQ(initial_text_frame + frame_index + 1, fixture.m_RenderContext->m_TextContext.m_Frame);
                for (uint32_t draw = 0; draw < draws_per_frame; ++draw)
                {
                    ASSERT_EQ(captured_value, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW,
                                                       frame_index * draws_per_frame + draw).m_Value);
                }

                // Cleanup follows the final draw but does not retire the packet.
                ASSERT_EQ(EVENT_TYPE_RENDER_END_FRAME, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 1].m_EventType);
                ASSERT_EQ(frame_index, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
                ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
                ASSERT_EQ(EVENT_TYPE_FRAME_RELEASE, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 1].m_EventType);
                dmGameSystem::EndRender(fixture.m_Context);
                ASSERT_EQ(initial_text_frame + frame_index + 1, fixture.m_RenderContext->m_TextContext.m_Frame);
            }

            DeleteCollection(&fixture, collection);
            if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
            {
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
                dmRender::StopRenderFrameQueue(queue);
                ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
            }
            else
            {
                dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);
            }
            ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
            ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
            DeleteComponentRenderFixture(&fixture);
        }
    }
}

// Verify recording and cancelling the next frame cannot reset timing or clean up a frame still being drawn.
TEST(ComponentRender, ProducerFrameBoundariesLeaveReadingFrameUntouched)
{
    if (!HasThreads())
    {
        return;
    }

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 1, true, true);
    fixture.m_Probe.m_HoldDraw = true;
    Executor executor;
    StartExecutor(&executor, &fixture);
    dmGameObject::HCollection collection = NewCollection(&fixture, "producer_boundaries_while_drawing");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
    const uint32_t initial_text_frame = fixture.m_RenderContext->m_TextContext.m_Frame;

    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    dmGameSystem::BeginRender(fixture.m_Context, 1.0f, 0.125f);
    ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
    ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.125f, 0));
    RequestPump(&executor, true);
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (!fixture.m_Probe.m_DrawEntered)
    {
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    }
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    // The renderer is paused inside the first draw. The game finishes its first
    // update, then starts and cancels the next packet without touching that draw.
    ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    dmGameSystem::EndRender(fixture.m_Context);
    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    dmGameSystem::BeginRender(fixture.m_Context, 2.0f, 0.25f);
    fixture.m_Types[0].m_FailCapture = true;
    ASSERT_FALSE(dmGameSystem::Render(fixture.m_Context, collection));
    ASSERT_TRUE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    dmGameSystem::EndRender(fixture.m_Context);

    ASSERT_EQ(2.0f, fixture.m_RenderContext->m_Recording.m_Time);
    ASSERT_EQ(0.25f, fixture.m_RenderContext->m_Recording.m_Dt);
    ASSERT_EQ(1.0f, fixture.m_RenderContext->m_Time);
    ASSERT_EQ(0.125f, fixture.m_RenderContext->m_Dt);
    ASSERT_EQ(initial_text_frame, fixture.m_RenderContext->m_TextContext.m_Frame);
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
    ASSERT_EQ((uint64_t)1, dmRender::GetRenderFrameQueueStatus(queue).m_Published);

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseDraw = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, WaitPump(&executor));

    ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, 0).m_Value);
    ASSERT_EQ(initial_text_frame, fixture.m_Probe.m_TextFrameAtEnd);
    ASSERT_EQ(initial_text_frame + 1, fixture.m_RenderContext->m_TextContext.m_Frame);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
    ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));

    DeleteCollection(&fixture, collection);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
    dmRender::StopRenderFrameQueue(queue);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteComponentRenderFixture(&fixture);
}

// Verify accepted overlapping frames keep their own cameras, culling and draw order across repeated slot reuse and world deletion.
TEST(ComponentRender, AcceptedOverlappingFramesKeepSnapshotViewsIsolated)
{
    if (!HasThreads())
    {
        return;
    }

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 2, false, true);
    fixture.m_Probe.m_HoldDraw = true;
    dmGameSystem::ComponentRenderType type;
    type.m_Capture = CaptureSnapshotItems;
    type.m_RenderCallbacks.m_Initialize = InitializeSnapshotRenderer;
    type.m_RenderCallbacks.m_Consume = ConsumeSnapshotItems;
    type.m_RenderCallbacks.m_EndFrame = EndSnapshotFrame;
    type.m_RenderCallbacks.m_Destroy = DeleteSnapshotRenderer;
    type.m_RenderContext = &fixture.m_Probe;
    for (uint32_t component = 0; component < 2; ++component)
    {
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::RegisterComponentRenderType(fixture.m_Render, component, type));
    }

    Executor executor;
    StartExecutor(&executor, &fixture);
    dmGameObject::HCollection collection = NewCollection(&fixture, "accepted_overlapping_views");
    ASSERT_NE((dmGameObject::HCollection)0, collection);
    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(fixture.m_Render);
    dmRender::RenderFrameTicket tickets[4];
    const uint32_t initial_text_frame = fixture.m_RenderContext->m_TextContext.m_Frame;
    RecordOverlapFrame(&fixture, collection, 0, &tickets[0]);
    ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, tickets[0]));

    for (uint32_t frame_index = 0; frame_index < DM_ARRAY_SIZE(tickets); ++frame_index)
    {
        dmMutex::Lock(fixture.m_Probe.m_Mutex);
        fixture.m_Probe.m_DrawEntered = false;
        fixture.m_Probe.m_ReleaseDraw = false;
        dmMutex::Unlock(fixture.m_Probe.m_Mutex);
        RequestPump(&executor, true);
        dmMutex::Lock(fixture.m_Probe.m_Mutex);
        while (!fixture.m_Probe.m_DrawEntered)
        {
            dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
        }
        dmMutex::Unlock(fixture.m_Probe.m_Mutex);

        const bool has_next = frame_index + 1 < DM_ARRAY_SIZE(tickets);
        if (has_next)
        {
            // Capture all of N+1 while N is inside its first batch. Publication
            // waits for N's acknowledgement instead of replacing accepted work.
            RecordOverlapFrame(&fixture, collection, frame_index + 1, &tickets[frame_index + 1]);
            ASSERT_NE(tickets[frame_index].m_Frame, tickets[frame_index + 1].m_Frame);
            ASSERT_EQ(dmRender::RENDER_FRAME_WOULD_BLOCK, dmRender::PublishRenderFrame(queue, tickets[frame_index + 1]));
            if (frame_index > 0)
            {
                ASSERT_EQ(tickets[frame_index - 1].m_Frame, tickets[frame_index + 1].m_Frame);
                ASSERT_NE(tickets[frame_index - 1].m_FrameId, tickets[frame_index + 1].m_FrameId);
            }
            ASSERT_EQ((float)frame_index + 2, fixture.m_RenderContext->m_Recording.m_Time);
            if (frame_index + 2 == DM_ARRAY_SIZE(tickets))
            {
                DeleteCollection(&fixture, collection);
                collection = 0;
            }
        }
        dmGameSystem::EndRender(fixture.m_Context);

        // Consumer timing, visibility and scratch data still belong to N; the
        // producer's end boundary must not clear it or finish component bindings.
        ASSERT_EQ((float)frame_index + 1, fixture.m_RenderContext->m_Time);
        ASSERT_EQ(initial_text_frame + frame_index, fixture.m_RenderContext->m_TextContext.m_Frame);
        ASSERT_EQ(2 * frame_index, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
        ASSERT_EQ(6 * frame_index, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
        ASSERT_EQ((uint64_t)frame_index + 1, dmRender::GetRenderFrameQueueStatus(queue).m_Published);

        dmMutex::Lock(fixture.m_Probe.m_Mutex);
        fixture.m_Probe.m_ReleaseDraw = true;
        dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
        dmMutex::Unlock(fixture.m_Probe.m_Mutex);
        ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, WaitPump(&executor));

        ASSERT_EQ(6 * (frame_index + 1), CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
        ASSERT_EQ(3 * (frame_index + 1), CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CULL));
        ASSERT_EQ(2 * (frame_index + 1), CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
        ASSERT_EQ(initial_text_frame + frame_index + 1, fixture.m_RenderContext->m_TextContext.m_Frame);
        ASSERT_EQ(-10.0f * frame_index, dmRender::GetViewMatrix(fixture.m_RenderContext).getTranslation().getX());
        for (uint32_t draw = 0; draw < 6; ++draw)
        {
            const uint32_t component = frame_index % 2 ? 1 - draw % 2 : draw % 2;
            const uint32_t item = draw / 2 == 1 ? 1 : 0;
            Event event = GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, 6 * frame_index + draw);
            ASSERT_EQ(component, event.m_ComponentIndex);
            ASSERT_EQ(item, event.m_World);
            ASSERT_EQ(1000 * frame_index + 10 * (component + 1) + item, event.m_Value);
            ASSERT_EQ(10.0f * frame_index + 4.0f * item + (component ? 0.25f : -0.5f), event.m_X);
        }

        ASSERT_EQ(1u, dmRender::RetireRenderFrames(queue));
        if (has_next)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_OK, dmRender::PublishRenderFrame(queue, tickets[frame_index + 1]));
        }
    }

    dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(queue);
    ASSERT_EQ((uint64_t)DM_ARRAY_SIZE(tickets), status.m_Published);
    ASSERT_EQ(status.m_Published, status.m_Consumed);
    ASSERT_EQ(status.m_Published, status.m_Retired);
    ASSERT_EQ((uint64_t)0, status.m_Cancelled);
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_GAME_DELETE));
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    dmRender::StopRenderFrameQueue(queue);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteComponentRenderFixture(&fixture);
}

// Verify a late draw reads published input after game-world deletion and ends before retirement destroys renderer state.
TEST(ComponentRender, LateDrawSurvivesGameWorldDeletion)
{
    if (!HasThreads())
    {
        return;
    }

    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_THREADED, 1, true, true);
    fixture.m_Types[0].m_ObserveRetirement = true;
    fixture.m_Probe.m_HoldDraw = true;
    Executor executor;
    StartExecutor(&executor, &fixture);
    dmGameObject::HCollection collection = NewCollection(&fixture, "deleted_before_late_draw");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
    dmGameSystem::BeginRender(fixture.m_Context, 1.0f, 0.125f);
    ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
    ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.125f, 0));
    RequestPump(&executor, true);
    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    while (!fixture.m_Probe.m_DrawEntered)
    {
        dmConditionVariable::Wait(fixture.m_Probe.m_Changed, fixture.m_Probe.m_Mutex);
    }
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);

    GetWorld(collection)->m_Value = 999;
    DeleteCollection(&fixture, collection);
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));

    dmMutex::Lock(fixture.m_Probe.m_Mutex);
    fixture.m_Probe.m_ReleaseDraw = true;
    dmConditionVariable::Broadcast(fixture.m_Probe.m_Changed);
    dmMutex::Unlock(fixture.m_Probe.m_Mutex);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, WaitPump(&executor));

    ASSERT_EQ(10u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, 0).m_Value);
    ASSERT_EQ(1u, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW, 0).m_World);
    ASSERT_EQ(EVENT_TYPE_RENDER_DRAW, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 2].m_EventType);
    ASSERT_EQ(EVENT_TYPE_RENDER_END_FRAME, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 1].m_EventType);
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    ASSERT_EQ(EVENT_TYPE_FRAME_RELEASE, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 1].m_EventType);
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
    ASSERT_EQ(EVENT_TYPE_RENDER_DELETE, fixture.m_Probe.m_Events[fixture.m_Probe.m_EventCount - 1].m_EventType);
    dmRender::StopRenderFrameQueue(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
    ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
    DeleteComponentRenderFixture(&fixture);
}

// Verify component preparation errors still execute accepted passes and end every initialized component before retirement.
TEST(ComponentRender, FailedPreparationStillReplaysPassesAndEnds)
{
    const dmRender::ExecutionMode modes[] = { dmRender::EXECUTION_MODE_INLINE, dmRender::EXECUTION_MODE_THREADED };
    const uint32_t mode_count = HasThreads() ? 2 : 1;
    for (uint32_t mode = 0; mode < mode_count; ++mode)
    {
        ComponentRenderFixture fixture;
        NewComponentRenderFixture(&fixture, modes[mode], 2, true, true);
        fixture.m_Probe.m_FailRenderConsume = true;
        fixture.m_Types[0].m_ObserveRetirement = true;
        fixture.m_Types[1].m_ObserveRetirement = true;
        Executor executor;
        if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
        {
            StartExecutor(&executor, &fixture);
        }
        dmGameObject::HCollection collection = NewCollection(&fixture, "failed_frame_preparation");
        ASSERT_NE((dmGameObject::HCollection)0, collection);
        const uint32_t initial_text_frame = fixture.m_RenderContext->m_TextContext.m_Frame;

        ASSERT_TRUE(dmGameSystem::BeginFrame(fixture.m_Context));
        dmGameSystem::BeginRender(fixture.m_Context, 1.0f, 0.125f);
        ASSERT_TRUE(dmGameSystem::Render(fixture.m_Context, collection));
        if (modes[mode] == dmRender::EXECUTION_MODE_INLINE)
        {
            dmGraphics::BeginFrame(fixture.m_Graphics);
        }
        ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.125f, 0));
        if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_CONSUMED, Pump(&executor, true));
        }

        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DRAW));
        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
        const uint32_t event_count = fixture.m_Probe.m_EventCount;
        ASSERT_EQ(EVENT_TYPE_RENDER_DRAW, fixture.m_Probe.m_Events[event_count - 4].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_DRAW, fixture.m_Probe.m_Events[event_count - 3].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_END_FRAME, fixture.m_Probe.m_Events[event_count - 2].m_EventType);
        ASSERT_EQ(EVENT_TYPE_RENDER_END_FRAME, fixture.m_Probe.m_Events[event_count - 1].m_EventType);
        ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(initial_text_frame, fixture.m_Probe.m_TextFrameAtEnd);
        ASSERT_EQ(initial_text_frame + 1, fixture.m_RenderContext->m_TextContext.m_Frame);

        ASSERT_FALSE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
        dmGameSystem::EndRender(fixture.m_Context);
        ASSERT_EQ(initial_text_frame + 1, fixture.m_RenderContext->m_TextContext.m_Frame);
        ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_FRAME_RELEASE));
        ASSERT_EQ(dmRender::RENDER_FRAME_INVALID_ARGUMENT, dmGameSystem::GetComponentRenderResult(fixture.m_Render));
        DeleteCollection(&fixture, collection);
        if (modes[mode] == dmRender::EXECUTION_MODE_THREADED)
        {
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_IDLE, Pump(&executor, false));
            dmRender::StopRenderFrameQueue(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
            ASSERT_EQ(dmRender::RENDER_FRAME_PUMP_DRAINED, Pump(&executor, true));
        }
        ASSERT_FALSE(fixture.m_Probe.m_WrongOwner);
        DeleteComponentRenderFixture(&fixture);
    }
}

// Verify legacy preparation preserves light submissions and keeps cleanup after PostUpdate, even for skipped or cancelled frames.
TEST(ComponentRender, DefaultPassWithoutComponents)
{
    dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 20;
    window_params.m_Height = 10;
    HWindow window = dmPlatform::NewWindow();
    dmPlatform::OpenWindow(window, window_params);

    dmGraphics::ContextParams graphics_params;
    graphics_params.m_Width = window_params.m_Width;
    graphics_params.m_Height = window_params.m_Height;
    graphics_params.m_Window = window;
    dmGraphics::HContext graphics = dmGraphics::NewContext(graphics_params);
    ASSERT_NE((dmGraphics::HContext)0, graphics);

    dmScript::ContextParams script_params = {};
    script_params.m_GraphicsContext = graphics;
    dmScript::HContext script = dmScript::NewContext(script_params);
    dmScript::Initialize(script);

    dmRender::RenderContextParams render_params;
    render_params.m_ScriptContext = script;
    render_params.m_MaxInstances = 8;
    render_params.m_MaxCharacters = 8;
    render_params.m_MaxBatches = 1;
    dmRender::HRenderContext render = dmRender::NewRenderContext(graphics, render_params);
    ASSERT_NE((dmRender::HRenderContext)0, render);
    dmRender::SetLightBufferCount(render, 1);
    dmRender::LightPrototypeParams light_params;
    dmRender::HLightPrototype light_prototype = dmRender::NewLightPrototype(render, light_params);
    dmRender::HLightInstance light = dmRender::NewLightInstance(render, light_prototype);
    ASSERT_NE((dmRender::HLightInstance)0, light);

    dmResource::NewFactoryParams factory_params;
    factory_params.m_MaxResources = 16;
    factory_params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
    dmResource::HFactory factory = dmResource::NewFactory(&factory_params, ".");
    ASSERT_NE((dmResource::HFactory)0, factory);
    dmGameObject::HRegister component_register = dmGameObject::NewRegister();
    dmGameObject::Initialize(component_register, script);
    dmGameSystem::ContextParams context_params = {};
    context_params.m_Factory = factory;
    context_params.m_Register = component_register;
    context_params.m_RenderContext = render;
    context_params.m_ScriptContext = script;
    dmGameSystem::HContext context = dmGameSystem::NewContext(context_params);
    ASSERT_NE((dmGameSystem::HContext)0, context);
    ASSERT_EQ(0u, dmGameObject::GetNumComponentTypes(component_register));
    dmRender::HRenderFrameQueue queue = dmGameSystem::GetComponentRenderQueue(context->m_ComponentRenderContext);
    ASSERT_NE((dmRender::HRenderFrameQueue)0, queue);
    const uint32_t initial_text_frame = render->m_TextContext.m_Frame;
    dmGameObject::HCollection collection = dmGameObject::NewCollection("legacy_frame_boundaries", factory, component_register, 8, 0);
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    dmRender::RenderListEntry* previous_entry = dmRender::RenderListAlloc(render, 1);
    dmRender::RenderListSubmit(render, previous_entry, previous_entry + 1);
    ASSERT_TRUE(dmGameSystem::BeginFrame(context));
    dmGameSystem::BeginRender(context, 3.25f, 0.125f);
    ASSERT_EQ(1u, render->m_TransientRenderList.Size());
    // Legacy light components submit before pass execution. Applying the packet's
    // timing must not call BeginFrame again and clear those submissions.
    dmRender::SubmitLightInstance(render, light);
    // List reset stays after the PreRender boundary, immediately before traversal.
    ASSERT_TRUE(dmGameSystem::Render(context, collection));
    ASSERT_EQ(0u, render->m_TransientRenderList.Size());
    ASSERT_EQ(1u, render->m_LightBufferSubmitted[0]);
    ASSERT_EQ(3.25f, render->m_Time);
    ASSERT_EQ(0.125f, render->m_Dt);
    // Leave execution timing stale to verify consumption applies the recorded values.
    render->m_Time = 0.0f;
    render->m_Dt = 0.0f;
    dmGraphics::BeginFrame(graphics);
    ASSERT_TRUE(dmGameSystem::SubmitRender(context, 0, 0.125f, 0x44332211));

    ASSERT_EQ(3.25f, render->m_Time);
    ASSERT_EQ(0.125f, render->m_Dt);
    ASSERT_EQ(1u, render->m_LightBufferSubmitted[0]);
    ASSERT_EQ(initial_text_frame, render->m_TextContext.m_Frame);
    dmGraphics::NullContext* null_graphics = (dmGraphics::NullContext*)graphics;
    ASSERT_EQ(0x11223344u, ((uint32_t*)null_graphics->m_MainFrameBuffer.m_ColorBuffer[0])[0]);
    ASSERT_EQ(1.0f, ((float*)null_graphics->m_MainFrameBuffer.m_DepthBuffer)[0]);
    dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(queue);
    ASSERT_EQ((uint64_t)1, status.m_Published);
    ASSERT_EQ((uint64_t)1, status.m_Consumed);
    ASSERT_EQ(1u, null_graphics->m_BeginFrameCount);
    dmGraphics::Flip(graphics);
    ASSERT_EQ(1u, null_graphics->m_FlipCount);

    ASSERT_TRUE(dmGameSystem::PostUpdate(context, 0));
    status = dmRender::GetRenderFrameQueueStatus(queue);
    ASSERT_EQ((uint64_t)1, status.m_Retired);
    ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[0]);
    ASSERT_EQ(dmRender::RENDER_FRAME_FREE, status.m_States[1]);
    ASSERT_EQ(initial_text_frame, render->m_TextContext.m_Frame);
    dmGameSystem::EndRender(context);
    ASSERT_EQ(initial_text_frame + 1, render->m_TextContext.m_Frame);

    // Iconified updates may reserve a packet without recording any rendering.
    ASSERT_TRUE(dmGameSystem::BeginFrame(context));
    ASSERT_TRUE(dmGameSystem::PostUpdate(context, 0));
    dmGameSystem::EndRender(context);
    ASSERT_EQ(initial_text_frame + 2, render->m_TextContext.m_Frame);

    // A recording failure also needs the legacy cleanup, despite no consumption.
    ASSERT_TRUE(dmGameSystem::BeginFrame(context));
    dmGameSystem::BeginRender(context, 4.0f, 0.125f);
    ASSERT_EQ(0u, render->m_LightBufferSubmitted[0]);
    dmRender::FailRenderFrame(dmGameSystem::GetComponentRenderFrame(context->m_ComponentRenderContext), dmRender::RENDER_FRAME_INVALID_ARGUMENT);
    ASSERT_FALSE(dmGameSystem::SubmitRender(context, 0, 0.125f, 0));
    ASSERT_TRUE(dmGameSystem::PostUpdate(context, 0));
    dmGameSystem::EndRender(context);
    ASSERT_EQ(initial_text_frame + 3, render->m_TextContext.m_Frame);
    ASSERT_EQ((uint64_t)1, dmRender::GetRenderFrameQueueStatus(queue).m_Consumed);

    dmGameObject::DeleteCollection(collection);
    dmGameObject::PostUpdate(component_register);
    ASSERT_TRUE(dmGameSystem::Finalize(context));
    dmGameSystem::DeleteContext(context);
    dmGameObject::DeleteRegister(component_register);
    dmRender::DeleteLightInstance(render, light);
    dmRender::DeleteLightPrototype(render, light_prototype);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::DeleteRenderContext(render, script));
    dmScript::Finalize(script);
    dmScript::DeleteContext(script);
    dmResource::DeleteFactory(factory);
    dmGraphics::DeleteContext(graphics);
    dmPlatform::CloseWindow(window);
    dmPlatform::DeleteWindow(window);
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
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));
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
    ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));

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
    ASSERT_TRUE(dmGameSystem::SubmitRender(fixture.m_Context, 0, 0.0f, 0));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_CAPTURE));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_LEGACY_RENDER));
    ASSERT_EQ(GetWorld(child)->m_Id, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_World);
    ASSERT_EQ(GetWorld(root)->m_Id, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 1).m_World);
    ASSERT_EQ(9.0f, GetEvent(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME, 0).m_X);
    ASSERT_NE((dmRender::HRenderFrameStorage)0, GetWorld(root)->m_LastCapturedFrame);
    ASSERT_EQ(GetWorld(root)->m_LastCapturedFrame, GetWorld(child)->m_LastCapturedFrame);
    ASSERT_EQ((dmRender::HRenderFrameStorage)0, GetWorld(disabled)->m_LastCapturedFrame);
    ASSERT_EQ((dmRender::HRenderFrameStorage)0, GetWorld(unrelated)->m_LastCapturedFrame);

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
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
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

// Verify an accepted frame's initialization error surfaces at frame completion and is not retried by later frames.
TEST(ComponentRender, RendererInitializationFailureIsNotRetried)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 1);
    fixture.m_Probe.m_FailRenderInit = true;

    dmGameObject::HCollection collection = NewCollection(&fixture, "failed_renderer_init");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));
    ASSERT_FALSE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    ASSERT_FALSE(dmGameSystem::BeginFrame(fixture.m_Context));

    // Submission reports acceptance, not the consumer's result. The engine must
    // not treat this packet as rejected and begin a second graphics frame.
    dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ((uint64_t)1, status.m_Published);
    ASSERT_EQ((uint64_t)1, status.m_Consumed);
    ASSERT_EQ((uint64_t)0, status.m_Cancelled);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CREATE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
    ASSERT_EQ(dmRender::RENDER_FRAME_INVALID_ARGUMENT,
        dmGameSystem::GetComponentRenderResult(fixture.m_Render));

    dmGameSystem::EndComponentRenderFrame(fixture.m_Render);
    DeleteCollection(&fixture, collection);
    dmGameSystem::ServiceComponentRenderContext(fixture.m_Render);

    ASSERT_EQ(1u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_DELETE));
    ASSERT_EQ(0u, dmGameSystem::GetComponentRenderWorldCount(fixture.m_Render));

    DeleteComponentRenderFixture(&fixture);
}

// Verify a consumer error does not turn acceptance into failure or skip other accepted entries and finalization.
TEST(ComponentRender, RendererConsumeErrorPreservesAcceptedEntries)
{
    ComponentRenderFixture fixture;
    NewComponentRenderFixture(&fixture, dmRender::EXECUTION_MODE_INLINE, 2);
    fixture.m_Probe.m_FailRenderConsume = true;

    dmGameObject::HCollection collection = NewCollection(&fixture, "failed_renderer_consume");
    ASSERT_NE((dmGameObject::HCollection)0, collection);

    ASSERT_TRUE(BeginAndRenderComponents(fixture.m_Context, collection));

    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_CONSUME));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_RENDER_END_FRAME));
    ASSERT_FALSE(dmGameSystem::PostUpdate(fixture.m_Context, collection));
    ASSERT_EQ(2u, CountEvents(&fixture.m_Probe, EVENT_TYPE_POST_UPDATE));
    ASSERT_FALSE(dmGameSystem::BeginFrame(fixture.m_Context));

    dmRender::RenderFrameQueueStatus status = dmRender::GetRenderFrameQueueStatus(dmGameSystem::GetComponentRenderQueue(fixture.m_Render));
    ASSERT_EQ((uint64_t)1, status.m_Published);
    ASSERT_EQ((uint64_t)1, status.m_Consumed);
    ASSERT_EQ((uint64_t)0, status.m_Cancelled);

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
